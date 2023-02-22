void ptrSwap(void** lpp, void** rpp) {
  if (lpp == rpp) {
    return;
  }
  void* ptr = *lpp;
  *lpp = *rpp;
  *rpp = ptr;
}