/*
=== Kraken Decompressor ===
Copyright (C) 2016, Powzix
Copyright (C) 2019, rarten
Copyright (C) 2022, Kerilk

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "stdafx.h"
#include "kraken.h"
#include <sys/stat.h>

// The decompressor will write outside of the target buffer.
#define SAFE_SPACE 64

void error(const char *s, const char *curfile = NULL) {
  if (curfile)
    fprintf(stderr, "%s: ", curfile);
  fprintf(stderr, "%s\n", s);
  exit(1);
}


byte *load_file(const char *filename, size_t *size) {
  FILE *f = fopen(filename, "rb");
  if (!f) error("file open error", filename);
  fseek(f, 0, SEEK_END);
  ssize_t packed_size = ftell(f);
  if (packed_size<0) error("file tell error", filename);
  fseek(f, 0, SEEK_SET);
  byte *input = new byte[packed_size];
  if (!input) error("memory error", filename);
  if (fread(input, 1, packed_size, f) != packed_size) error("error reading", filename);
  fclose(f);
  *size = (size_t)packed_size;
  return input;
}

enum {
  kCompressor_Kraken = 8,
  kCompressor_Mermaid = 9,
  kCompressor_Selkie = 11,
  kCompressor_Hydra = 12,
  kCompressor_Leviathan = 13,
};

bool arg_stdout, arg_force, arg_quiet, arg_dll;
int arg_compressor = kCompressor_Kraken, arg_level = 4;
char arg_direction;
const char *verifyfolder;

int ParseCmdLine(int argc, char *argv[]) {
  int i;
  // parse command line
  for (i = 1; i < argc; i++) {
    const char *s = argv[i];
    char c;
    if (*s != '-')
      break;
    if (*++s == '-') {
      if (*++s == 0) {
        i++;
        break;  // --
      }
      // long opts
      if (!strcmp(s, "stdout")) s = "c";
      else if (!strcmp(s, "decompress")) s = "d";
      else if (!strcmp(s, "compress")) s = "z";
      else if (!strncmp(s, "verify=",7)) {
        verifyfolder = s + 7;
        continue;
      } else if (!strcmp(s, "verify")) {
        arg_direction = 't';
        continue;
      } else if (!strcmp(s, "dll")) {
        arg_dll = true;
        continue;
      } else if (!strcmp(s, "kraken")) s = "mk";
      else if (!strcmp(s, "mermaid")) s = "mm";
      else if (!strcmp(s, "selkie")) s = "ms";
      else if (!strcmp(s, "leviathan")) s = "ml";
      else if (!strcmp(s, "hydra")) s = "mh";
      else if (!strncmp(s, "level=", 6)) {
        arg_level = atoi(s + 6);
        continue;
      } else {
        return -1;
      }
    }
    // short opt
    do {
      switch (c = *s++) {
      case 'z':
      case 'd':
      case 'b':
        if (arg_direction)
          return -1;
        arg_direction = c;
        break;
      case 'c':
        arg_stdout = true;
        break;
      case 'f':
        arg_force = true;
        break;
      case 'q':
        arg_quiet = true;
        break;
      case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
        arg_level = c - '0';
        break;
      case 'm':
        c = *s++;
        arg_compressor = (c == 'k') ? kCompressor_Kraken :
                         (c == 'm') ? kCompressor_Mermaid : 
                         (c == 's') ? kCompressor_Selkie : 
                         (c == 'l') ? kCompressor_Leviathan : 
                         (c == 'h') ? kCompressor_Hydra : -1;
        if (arg_compressor < 0)
          return -1;
        break;
      default:
        return -1;
      }
    } while (*s);
  }
  return i;
}

bool Verify(const char *filename, uint8 *output, size_t outbytes, const char *curfile) {
  size_t test_size;
  byte *test = load_file(filename, &test_size);
  if (!test) {
    fprintf(stderr, "file open error: %s\n", filename);
    return false;
  }
  if (test_size != outbytes) {
    fprintf(stderr, "%s: ERROR: File size difference: %zu vs %zu\n", filename, outbytes, test_size);
    return false;
  }
  for (int i = 0; i != test_size; i++) {
    if (test[i] != output[i]) {
      fprintf(stderr, "%s: ERROR: File difference at 0x%x. Was %d instead of %d\n", curfile, i, output[i], test[i]);
      return false;
    }
  }
  return true;
}

#ifndef _MSC_VER
typedef uint64_t LARGE_INTEGER;
void QueryPerformanceCounter(LARGE_INTEGER *a) {
  *a = 0;
}
void QueryPerformanceFrequency(LARGE_INTEGER *a) {
  *a = 1;
}
#define WINAPI
typedef void* HINSTANCE;
HINSTANCE LoadLibraryA(const char *s) { return 0; }
void *GetProcAddress(HINSTANCE h, const char *s) { return 0; }
#endif

typedef ssize_t WINAPI OodLZ_CompressFunc(
  int codec, uint8 *src_buf, size_t src_len, uint8 *dst_buf, int level,
  void *opts, size_t offs, size_t unused, void *scratch, size_t scratch_size);
typedef ssize_t WINAPI OodLZ_DecompressFunc(uint8 *src_buf, int src_len, uint8 *dst, size_t dst_size,
                                          int fuzz, int crc, int verbose,
                                          uint8 *dst_base, size_t e, void *cb, void *cb_ctx, void *scratch, size_t scratch_size, int threadPhase);

OodLZ_CompressFunc *OodLZ_Compress;
OodLZ_DecompressFunc *OodLZ_Decompress;

void LoadLib() {

#if defined(_M_X64)
#define LIBNAME "oo2core_7_win64.dll"
  char COMPFUNCNAME[] = "XXdleLZ_Compress";
  char DECFUNCNAME[] = "XXdleLZ_Decompress";
  COMPFUNCNAME[0] = DECFUNCNAME[0] = 'O';
  COMPFUNCNAME[1] = DECFUNCNAME[1] = 'o';
#else
#define LIBNAME "oo2core_7_win32.dll"
  char COMPFUNCNAME[] = "_XXdleLZ_Compress@40";
  char DECFUNCNAME[] = "_XXdleLZ_Decompress@56";
  COMPFUNCNAME[1] = DECFUNCNAME[1] = 'O';
  COMPFUNCNAME[2] = DECFUNCNAME[2] = 'o';
#endif
  HINSTANCE mod = LoadLibraryA(LIBNAME);
  OodLZ_Compress = (OodLZ_CompressFunc*)GetProcAddress(mod, COMPFUNCNAME);
  OodLZ_Decompress = (OodLZ_DecompressFunc*)GetProcAddress(mod, DECFUNCNAME);
  if (!OodLZ_Compress || !OodLZ_Decompress)
    error("error loading", LIBNAME);
}

int main(int argc, char *argv[]) {
  int64_t start, end, freq;
  int argi;

  if (argc < 2 || 
      (argi = ParseCmdLine(argc, argv)) < 0 || 
      argi >= argc ||  // no files
      (arg_direction != 'b' && (argc - argi) > 2) ||  // too many files
      (arg_direction == 't' && (argc - argi) != 2)     // missing argument for verify
      ) {
    fprintf(stderr, "ooz v7.0\n\n"
      "Usage: ooz [options] input [output]\n"
      " -c --stdout              write to stdout\n"
      " -d --decompress          decompress (default)\n"
      " -z --compress            compress (requires oo2core_7_win64.dll)\n"
      " -b                       just benchmark, don't overwrite anything\n"
      " -f                       force overwrite existing file\n"
      " --dll                    decompress with the dll\n"
      " --verify                 decompress and verify that it matches output\n"
      " --verify=<folder>        verify with files in this folder\n"
      " -<1-9> --level=<-4..10>  compression level\n"
      " -m<k>                    [k|m|s|l|h] compressor selection\n"
      " --kraken --mermaid --selkie --leviathan --hydra    compressor selection\n\n"
      "(Warning! not fuzz safe, so please trust the input)\n"
      );
    return 1;
  }
  bool write_mode = (argi + 1 < argc) && (arg_direction != 't' && arg_direction != 'b');

  if (!arg_force && write_mode) {
    struct stat sb;
    if (stat(argv[argi + 1], &sb) >= 0) {
      fprintf(stderr, "file %s already exists, skipping.\n", argv[argi + 1]);
      return 1;
    }
  }

  int nverify = 0;

  for (; argi < argc; argi++) {
    const char *curfile = argv[argi];

    size_t input_size;
    byte *input = load_file(curfile, &input_size);

    byte *output = NULL;
    ssize_t outbytes = 0;

    if (arg_direction == 'z') {
      // compress using the dll
      LoadLib();
      output = new byte[input_size + 65536];
      if (!output) error("memory error", curfile);
      *(uint64*)output = input_size;
      QueryPerformanceCounter((LARGE_INTEGER*)&start);
      outbytes = OodLZ_Compress(arg_compressor, input, input_size, output + 8, arg_level, 0, 0, 0, 0, 0);
      if (outbytes < 0) error("compress failed", curfile);
      outbytes += 8;
      QueryPerformanceCounter((LARGE_INTEGER*)&end);
      QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
      double seconds = (double)(end - start) / freq;
      if (!arg_quiet)
        fprintf(stderr, "%-20s: %8zu => %8zu (%.2f seconds, %.2f MB/s)\n", argv[argi], input_size, (size_t)outbytes, seconds, input_size * 1e-6 / seconds);
    } else {
      if (arg_dll)
        LoadLib();

      // stupidly attempt to autodetect if file uses 4-byte or 8-byte header,
      // the previous version of this tool wrote a 4-byte header.
      int hdrsize = *(uint64*)input >= 0x10000000000 ? 4 : 8;
      
      size_t unpacked_size = (hdrsize == 8) ? *(size_t*)input : *(uint32*)input;
      if (unpacked_size > (hdrsize == 4 ? 52*1024*1024 : 1024 * 1024 * 1024)) 
        error("file too large", curfile);
      output = new byte[unpacked_size + SAFE_SPACE];
      if (!output) error("memory error", curfile);

      QueryPerformanceCounter((LARGE_INTEGER*)&start);

      if (arg_dll) {
        outbytes = OodLZ_Decompress(input + hdrsize, input_size - hdrsize, output, unpacked_size, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
      } else {
        outbytes = Kraken_Decompress(input + hdrsize, input_size - hdrsize, output, unpacked_size);
      }
      if (outbytes != unpacked_size)
        error("decompress error", curfile);
      QueryPerformanceCounter((LARGE_INTEGER*)&end);
      QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
      double seconds = (double)(end - start) / freq;
      if (!arg_quiet)
        fprintf(stderr, "%-20s: %8zu => %8zu (%.2f seconds, %.2f MB/s)\n", argv[argi], input_size, unpacked_size, seconds, unpacked_size * 1e-6 / seconds);
    }

    if (verifyfolder) {
      // Verify against the file in verifyfolder with the same basename excluding extension
      char buf[1024];
      const char *basename = curfile;
      for(const char *s = curfile; *s; s++)
        if (*s == '/' || *s == '\\')
          basename = s + 1;
      const char *ext = strrchr(basename, '.');
      snprintf(buf, sizeof(buf), "%s/%.*s", verifyfolder, (int)(ext ? (ext - basename) : strlen(basename)), basename);
      if (!Verify(buf, output, outbytes, curfile))
        return 1;
      nverify++;
    }

    if (arg_stdout && arg_direction != 't' && arg_direction != 'b')
      fwrite(output, 1, outbytes, stdout);

    if (write_mode) {
      if (arg_direction == 't') {
        if (!Verify(argv[argi + 1], output, outbytes, curfile))
          return 1;
        fprintf(stderr, "%s: Verify OK\n", curfile);
      } else {
        FILE *f = fopen(argv[argi + 1], "wb");
        if (!f) error("file open for write error", argv[argi + 1]);
        if (fwrite(output, 1, outbytes, f) != outbytes)
          error("file write error", argv[argi + 1]);
        fclose(f);
      }
      break;
    }
    delete[] input;
    delete[] output;
  }

  if (nverify)
    fprintf(stderr, "%d files verified OK!\n", nverify);
  return 0;
}

