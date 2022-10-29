#include <ruby.h>
#include "kraken.h"
#include <stdlib.h>

VALUE mOodleKraken;

#define SAFE_SPACE 64

static VALUE  decompress_p(VALUE self, VALUE in_str, VALUE decompressed_size) {
  (void)self;
  VALUE out_str = Qnil;
  Check_Type(in_str, T_STRING);
  size_t out_sz = NUM2ULL(decompressed_size);
  size_t in_sz = RSTRING_LEN(in_str);
  const uint8_t *in_ptr = RSTRING_PTR(in_str);

  uint8_t *out_ptr = malloc((long)out_sz + SAFE_SPACE);
  if (!out_ptr)
    rb_raise(rb_eRuntimeError, "Decompression Error");
  ssize_t out_bytes = Kraken_Decompress(in_ptr, in_sz, out_ptr, out_sz);
  if (out_bytes < 0 || (size_t)out_bytes != out_sz)
    rb_raise(rb_eRuntimeError, "Decompression Error");
  out_str = rb_str_buf_new((long)out_sz);
  rb_str_cat(out_str, out_ptr, out_sz);
  return out_str;
}

void Init_oodle_kraken_c() {
  mOodleKraken = rb_define_module("OodleKraken");
  /**
   * @overload decompress(str, decompressed_size)
   * decompress an Oodle compressed string
   * @param str [String] the string to decompress
   * @param decompressed_size [Integer] the size of the decompressed data
   * @return [String] the decompressed string
   */
  rb_define_module_function(mOodleKraken, "decompress", decompress_p, 2);
}
