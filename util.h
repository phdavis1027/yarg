#ifndef _YARG_UTIL_H_
#define _YARG_UTIL_H_

#define HMRecord(name, kt, vt) \
    typedef struct { \
	kt key; \
	vt value; \
    } name; \
    typedef name* HM_##name;

#define DA(t) \
  typedef t* DA_##t;

#define hash_map(t) t*;
#define d_array(t) t*;

#endif
