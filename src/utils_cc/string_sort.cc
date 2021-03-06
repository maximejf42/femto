/*
  (*) 2014-2015 Michael Ferguson <michaelferguson@acm.org>

    * This is a work of the United States Government and is not protected by
      copyright in the United States.

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

  femto/src/utils_cc/string_sort.cc
*/



extern "C" {
  #include "string_sort.h"
  #include "bswap.h"
  #include "bit_funcs.h"
}
#include "varint.hh"
#include "bucket_sort.hh"

struct SortingProblemStringKey {
  unsigned char* str;
  size_t len;

  SortingProblemStringKey() : str(NULL), len(0) { }
  SortingProblemStringKey(unsigned char* data, size_t len) : str(data), len(len) { }

  // We could use any number of bytes as a key-part
  // Tests indicate it's slightly faster to use 8bytes,
  // which is probably because of the large records
  // in suffix sorting and the comparison going on
  // in the radix sort.

  //typedef uint8_t key_part_t;
  //typedef uint16_t key_part_t;
  //typedef uint32_t key_part_t;
  typedef uint64_t key_part_t;
  key_part_t get_key_part(size_t i) const
  {
    key_part_t tmp;
    memcpy(&tmp, str+i*sizeof(key_part_t), sizeof(key_part_t));
    if( sizeof(key_part_t) == 1 ) return tmp;
    if( sizeof(key_part_t) == 2 ) return ntoh_16(tmp);
    if( sizeof(key_part_t) == 4 ) return ntoh_32(tmp);
    if( sizeof(key_part_t) == 8 ) return ntoh_64(tmp);
  }

  size_t get_num_key_parts() const
  {
    return ceildiv(len,sizeof(key_part_t));
  }

};

template <int PtrBytes>
struct SortingProblemFixedLengthStringRecordSortingCriterion {
  typedef return_key_criterion_tag criterion_category;
  typedef SortingProblemStringKey key_t;

  void *context;
  const get_string_fun_t get_str;
  const size_t len;

  SortingProblemFixedLengthStringRecordSortingCriterion( string_sort_params_t *params ) : context(params->context), get_str(params->get_string), len(params->str_len) {

  }

  key_t get_key(const be_uint<PtrBytes>& r)
  {
    SortingProblemStringKey ret(get_str(context, &r), len);
    return ret;
  }
};

template <int PtrBytes>
struct SortingProblemCompare {
  typedef compare_criterion_tag criterion_category;

  void *context;
  const compare_fun_t cmp;

  SortingProblemCompare( string_sort_params_t *params ) : context(params->context), cmp(params->compare) { }
  int compare(const be_uint<PtrBytes>& a, const be_uint<PtrBytes>& b) {
    return (cmp)?(cmp(context, &a, &b)):(0);
  }
};

/*
    params.context = context;// context
    params.base = // base of pointer records
    params.n_memb = // how many pointer records
    params.memb_size ; // bytes per pointer record
    params.str_len ; // how many bytes to sort
    params.same_depth = 0; // not used here
    
    typedef string_t (*get_string_fun_t)(const void* context, const void* data_ptr);
    params.get_string = get_string_getter(context); // return a string
      in: context, pointer to record; out -> pointer to string bytes

      typedef int (*compare_fun_t)(const void* context, const void * a, const void * b);
     in: context, pointer to record, pointer to record; out -> cmp
    params.compare = compare;
*/


template<int PtrBytes>
void do_sorting_problem(string_sort_params_t *context)
{
  assert(context->memb_size == PtrBytes);
  
  SortingProblemFixedLengthStringRecordSortingCriterion<PtrBytes> cmp1(context);
  SortingProblemCompare<PtrBytes> cmp2(context);

  size_t len = context->str_len;
  // allocate extra space for 8-byte loads
  size_t alloc_len = len + 8;
  unsigned char* minbuf = (unsigned char*) malloc(alloc_len);
  unsigned char* maxbuf = (unsigned char*) malloc(alloc_len);
  memset(minbuf, 0, alloc_len);
  memset(maxbuf, 0xff, alloc_len);

  SortingProblemStringKey min(minbuf, len);
  SortingProblemStringKey max(maxbuf, len);

  be_uint<PtrBytes>* records = (be_uint<PtrBytes>*) context->base;
  sort_array_compare_after(cmp1, cmp2, min, max, records, context->n_memb, context->parallel != 0);

  free(minbuf);
  free(maxbuf);
}


error_t bucket_sort(string_sort_params_t* params)
{
  int ptrbytes = params->memb_size;
  if( ptrbytes == 1 ) do_sorting_problem<1>(params);
  else if( ptrbytes == 2 ) do_sorting_problem<2>(params);
  else if( ptrbytes == 3 ) do_sorting_problem<3>(params);
  else if( ptrbytes == 4 ) do_sorting_problem<4>(params);
  else if( ptrbytes == 5 ) do_sorting_problem<5>(params);
  else if( ptrbytes == 6 ) do_sorting_problem<6>(params);
  else if( ptrbytes == 7 ) do_sorting_problem<7>(params);
  else if( ptrbytes == 8 ) do_sorting_problem<8>(params);
  else return ERR_PARAM;

  return ERR_NOERR;
}

