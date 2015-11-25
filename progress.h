#ifndef PROGRESS_H_
#define PROGRESS_H_

#ifndef USING_ABT
inline void mpiv_recv_recv_ready(mpiv_packet* p) {
    // printf("sending: %p %d t: %p %d sz: %d\n", p->rdz.src_addr, p->rdz.lkey, p->rdz.tgt_addr,
    //    p->rdz.rkey, p->rdz.size);
    int rank = p->header.from;
    int tag = p->header.tag;
    // printf("%d recv RECV_READY FROM %d %d\n", MPIV.me, rank, tag);
    mpiv_key key = mpiv_make_key(rank, tag);

    void* buf;
    if (!MPIV.rdztbl.find(key, buf)) {
        if (MPIV.rdztbl.insert(key, p)) {
            // handle by thread.
            return;
        }
        buf = MPIV.rdztbl[key];
    }

    // Else, thread win, we handle.
    MPIV_Request* s = (MPIV_Request*) buf;
    p->header.type = SEND_READY_FIN;
    // printf("write rdma %d %d\n", rank, tag);

    MPIV.conn[rank].write_rdma_signal(s->buffer, MPIV.heap.lkey(),
        (void*) p->rdz.tgt_addr, p->rdz.rkey, p->rdz.size, p, (uint32_t) tag);
}

inline void mpiv_recv_recv_ready_finalize(const mpiv_key& key) {
    // Now data is already ready in the buffer.
    void *sync = NULL;
    MPIV_Request* fsync = NULL;
    sync = MPIV.tbl[key];
    fsync = (MPIV_Request*) sync;
    fsync->signal();
}

inline void mpiv_recv_inline(mpiv_packet* p_ctx, const ibv_wc& wc) {
    uint32_t recv_size = wc.byte_len;
    int tag = wc.imm_data;
    int rank = MPIV.dev_ctx->get_rank(wc.qp_num);
    mpiv_key key = mpiv_make_key(rank, tag);

    void *sync = NULL;
    MPIV_Request* fsync = NULL;
    if (!MPIV.tbl.find(key, sync)) {
        if (MPIV.tbl.insert(key, (void*) p_ctx)) {
            // This will be handle by the thread,
            // so we return right away.
            return;
        }
        fsync = (MPIV_Request*) (void*) MPIV.tbl[key];
    } else {
        fsync = (MPIV_Request*) sync;
    }
    memcpy(fsync->buffer, (void*) p_ctx, recv_size);
    fsync->signal();
    assert(MPIV.squeue.push(p_ctx));
}

inline void mpiv_recv_short(mpiv_packet* p, const ibv_wc& wc) {
    uint32_t recv_size = wc.byte_len - sizeof(mpiv_packet_header);
    int rank = p->header.from;
    int tag = p->header.tag;
    mpiv_key key = mpiv_make_key(rank, tag);

    void *sync = NULL;
    MPIV_Request* fsync = NULL;
    if (!MPIV.tbl.find(key, sync)) {
        if (MPIV.tbl.insert(key, (void*) p)) {
            // This will be handle by the thread,
            // so we return right away.
            return;
        }
        fsync = (MPIV_Request*) (void*) MPIV.tbl[key];
    } else {
        fsync = (MPIV_Request*) sync;
    }

    std::memcpy(fsync->buffer, p->egr.buffer, recv_size);
    fsync->signal();
    assert(MPIV.squeue.push(p));
}

inline void mpiv_recv_send_ready(mpiv_packet* p) {
    int rank = p->header.from;
    int tag = p->header.tag;
    // printf("%d recv from %d %d\n", MPIV.me, rank, tag);

    mpiv_key key = mpiv_make_key(rank, tag);
    void *sync = NULL;
    MPIV_Request* fsync = NULL;
    if (!MPIV.tbl.find(key, sync)) {
        if (MPIV.tbl.insert(key, (void*) p)) {
            // This will be handle by the thread,
            // so we return right away.
            return;
        }
        fsync = (MPIV_Request*) (void*) MPIV.tbl[key];
    } else {
        fsync = (MPIV_Request*) sync;
    }
    mpiv_send_recv_ready(p, fsync);
}

inline void mpiv_done_rdz_send(mpiv_packet* p) {
    int tag = p->header.tag;
    int rank = p->header.from;
    // printf("%d FININSH RDMA TO %d %d", MPIV.me, rank, tag);
    mpiv_key key = mpiv_make_key(rank, tag);
    void* buf = MPIV.rdztbl[key];
    MPIV_Request* s = (MPIV_Request*) buf;
    s->signal();
}

inline void MPIV_Progress() {
    // Poll recv queue.
    MPIV.dev_rcq.poll_once([](const ibv_wc& wc) {
        timing -= MPI_Wtime();
        mpiv_packet* p_ctx = (mpiv_packet*) wc.wr_id;
        if (wc.opcode == IBV_WC_RECV_RDMA_WITH_IMM) {
            // FIXME(danghvu): is there any other protocol that uses RDMA_WITH_IMM ?
            int tag = (int) wc.imm_data & 0xffffffff;
            int rank = (int) MPIV.dev_ctx->get_rank(wc.qp_num);
            // printf("%d recv rdma %d %d\n", MPIV.me, rank, tag);
            mpiv_recv_recv_ready_finalize(mpiv_make_key(rank, tag));
            // return the packet.
            assert(MPIV.squeue.push(p_ctx));
        } else if (wc.imm_data != (uint32_t)-1) {
            // This is INLINE.
            mpiv_recv_inline(p_ctx, wc);
        } else {
            const auto& type = p_ctx->header.type;
            // Other cases.
            if (type == SEND_SHORT) {
                mpiv_recv_short(p_ctx, wc);
            } else if (type == SEND_READY) {
                mpiv_recv_send_ready(p_ctx);
            } else if (type == RECV_READY) {
                mpiv_recv_recv_ready(p_ctx);
            } else {
                assert(0 && "Invalid packet or not implemented");
            }
        }
        // Post back a recv, as we have just consumed one.
        mpiv_post_recv(get_freesbuf());
        timing += MPI_Wtime();
    });

    // Poll send queue.
    MPIV.dev_scq.poll_once([](const ibv_wc& wc) {
        timing -= MPI_Wtime();
        if (wc.wr_id == 0) {
            // This is an INLINE, do nothing.
            return;
        }
        mpiv_packet* packet = (mpiv_packet*) wc.wr_id;
        const auto& type = packet->header.type;
        if (type == SEND_SHORT || type == SEND_READY || type == RECV_READY) {
            // done.
        } else if (type == SEND_READY_FIN) {
            mpiv_done_rdz_send(packet);
            // done rdz send.
        } else {
            assert(0 && "Invalid packet or not implemented");
        }
        // Return the packet, as we have just used one.
        assert(MPIV.squeue.push(packet));
        timing += MPI_Wtime();
    });
}
#endif

inline void MPI_Progress() {
    MPI_Status stat;
    int flag = 0;
    MPI_Iprobe(1, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &stat);

    // nothing.
    if (!flag) return;

    int tag = stat.MPI_TAG;
    mpiv_key key = mpiv_make_key(1, tag);

    void *sync = NULL;
    if (!MPIV.tbl.find(key, sync)) {
        // unmatched recv, allocate a buffer for it and continue.
        int size = 0;
        MPI_Get_count(&stat, MPI_BYTE, &size);
        void* buf = std::malloc(size);
        MPI_Recv(buf, size, MPI_BYTE, stat.MPI_SOURCE, tag,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        if (!MPIV.tbl.insert(key, buf)) {
            MPIV_Request* fsync = (MPIV_Request*) (void*) MPIV.tbl[key];
            memcpy(fsync->buffer, buf, size);
            free(buf);
            MPIV.tbl.erase(key);
            fsync->signal();
        } else {
            // this branch is handle
            // by the thread.
        }
    } else {
        MPIV_Request* fsync = (MPIV_Request*) sync;
        MPI_Recv(fsync->buffer, fsync->size, MPI_BYTE, 1, tag,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPIV.tbl.erase(key);
        fsync->signal();
    }
}

#endif