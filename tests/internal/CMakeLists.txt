add_lci_tests(
  TESTS
  run_internal_test.c
  LABELS
  internal
  COMMANDS
  "${SRUN_EXE} -n 2 ${SRUN_EXTRA_ARG} [TARGET]")
