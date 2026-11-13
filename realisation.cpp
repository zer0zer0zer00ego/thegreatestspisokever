
#include "dllist6.hh"


template class DList<int, HeapNodeSource<int>>;
template class DList<int, PoolNodeSource<int>>;
template class DList<int, PoolNodeSource<int, 4096>>;
template class DList<NoDefault, HeapNodeSource<NoDefault>>;
