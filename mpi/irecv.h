#ifndef IRECV_H_
#define IRECV_H_

extern int mpiv_recv_start, mpiv_recv_end;

extern int mpiv_worker_id();

void proto_recv_rndz(void* buffer, int size, int rank, int tag,
                     MPIV_Request* s);

void irecv(void* buffer, int count, MPI_Datatype datatype, int rank, int tag,
           MPI_Comm, MPIV_Request* req) {
  int size = 0;
  MPI_Type_size(datatype, &size);
  size *= count;
  new (req) MPIV_Request(buffer, size, rank, tag);
  if (size <= SHORT_MSG_SIZE) 
    req->type = REQ_RECV_SHORT;
  else {
    req->type = REQ_RECV_LONG;
    proto_recv_rndz(buffer, size, rank, tag, req);
  }
}

#endif