#include "lci.h"

int main(int argc, char *argv[])
{
  LCI_error_t ret = LCIX_internal_test(argc, argv);
  if (ret == LCI_OK) {
    printf("Internal test succeed!\n");
  } else {
    printf("Internal test failed!\n");
  }
  return 0;
}