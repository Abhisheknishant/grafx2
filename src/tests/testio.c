/* vim:expandtab:ts=2 sw=2:
*/
/*  Grafx2 - The Ultimate 256-color bitmap paint program

    Copyright 2018-2020 Thomas Bernard
    Copyright 2011 Pawel Góralski
    Copyright 2009 Petter Lindquist
    Copyright 2008 Yves Rizoud
    Copyright 2008 Franck Charlet
    Copyright 2007-2011 Adrien Destugues
    Copyright 1996-2001 Sunset Design (Guillaume Dorme & Karl Maritaud)

    Grafx2 is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; version 2
    of the License.

    Grafx2 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Grafx2; if not, see <http://www.gnu.org/licenses/>
*/
///@file testio.c
/// Unit tests.
///

#include <stdlib.h>
#include <string.h>
#include "tests.h"
#include "../struct.h"
#include "../io.h"
#include "../gfx2mem.h"
#include "../gfx2log.h"

// random()/srandom() not available with mingw32
#if defined(WIN32)
#define random (long)rand
#endif

int Test_Read_Write_byte(void)
{
  char path[256];
  FILE * f;
  byte b = 0;

  snprintf(path, sizeof(path), "%s%sbyte.bin", tmpdir, PATH_SEPARATOR);
  f = fopen(path, "w+b");
  if (f == NULL)
  {
    GFX2_Log(GFX2_ERROR, "error opening %s\n", path);
    return 0;
  }
  if (!Write_byte(f, 42))
  {
    GFX2_Log(GFX2_ERROR, "error writing\n");
    fclose(f);
    return 0;
  }
  rewind(f);
  if (!Read_byte(f, &b))
  {
    GFX2_Log(GFX2_ERROR, "error reading\n");
    fclose(f);
    return 0;
  }
  if (b != 42)
  {
    GFX2_Log(GFX2_ERROR, "Byte value mismatch\n");
    fclose(f);
    return 0;
  }
  fclose(f);
  Remove_path(path);
  return 1;
}

int Test_Read_Write_word(void)
{
  char path[256];
  FILE * f;
  word w1 = 0, w2 = 0;
  byte b = 0;

  snprintf(path, sizeof(path), "%s%sword.bin", tmpdir, PATH_SEPARATOR);
  f = fopen(path, "w+b");
  if (f == NULL)
  {
    GFX2_Log(GFX2_ERROR, "error opening %s\n", path);
    return 0;
  }
  // write bytes 12 34 56 78 90
  if (!Write_word_le(f, 0x3412) || !Write_word_be(f, 0x5678) || !Write_byte(f, 0x90))
  {
    GFX2_Log(GFX2_ERROR, "error writing\n");
    fclose(f);
    return 0;
  }
  rewind(f);
  if (!Read_byte(f, &b) || !Read_word_be(f, &w1) || !Read_word_le(f, &w2))
  {
    GFX2_Log(GFX2_ERROR, "error reading\n");
    fclose(f);
    return 0;
  }
  if ((b != 0x12) || (w1 != 0x3456) || (w2 != 0x9078))
  {
    GFX2_Log(GFX2_ERROR, "word values mismatch\n");
    fclose(f);
    return 0;
  }
  fclose(f);
  Remove_path(path);
  return 1;
}

int Test_Read_Write_dword(void)
{
  char path[256];
  FILE * f;
  dword dw1 = 0, dw2 = 0;
  byte b = 0;

  snprintf(path, sizeof(path), "%s%sdword.bin", tmpdir, PATH_SEPARATOR);
  f = fopen(path, "w+b");
  if (f == NULL)
  {
    GFX2_Log(GFX2_ERROR, "error opening %s\n", path);
    return 0;
  }
  // write bytes 01 02 03 04 05 06 07 08 09
  if (!Write_dword_le(f, 0x04030201) || !Write_dword_be(f, 0x05060708) || !Write_byte(f, 9))
  {
    GFX2_Log(GFX2_ERROR, "error writing\n");
    fclose(f);
    return 0;
  }
  rewind(f);
  if (!Read_byte(f, &b) || !Read_dword_be(f, &dw1) || !Read_dword_le(f, &dw2))
  {
    GFX2_Log(GFX2_ERROR, "error reading\n");
    fclose(f);
    return 0;
  }
  if ((b != 0x1) || (dw1 != 0x02030405) || (dw2 != 0x09080706))
  {
    GFX2_Log(GFX2_ERROR, "word values mismatch\n");
    fclose(f);
    return 0;
  }
  fclose(f);
  Remove_path(path);
  return 1;
}
int Test_Read_Write_bytes(void)
{
  char path[256];
  FILE * f;
  unsigned long len;
  byte * buffer;
  byte b;

  snprintf(path, sizeof(path), "%s%sbytes.bin", tmpdir, PATH_SEPARATOR);
  f = fopen(path, "w+b");
  if (f == NULL)
  {
    GFX2_Log(GFX2_ERROR, "error opening %s\n", path);
    return 0;
  }
  b = (byte)random();
  len = 1000 + (random() & 0x3ff);
  buffer = GFX2_malloc(len);
  if (buffer == NULL)
  {
    fclose(f);
    return 0;
  }
  memset(buffer, b, len);
  GFX2_Log(GFX2_DEBUG, "Writing %lu bytes 0x%02x\n", len, (int)b);
  // write bytes
  if (!Write_bytes(f, buffer, len))
  {
    GFX2_Log(GFX2_ERROR, "error writing\n");
    free(buffer);
    fclose(f);
    return 0;
  }
  rewind(f);
  memset(buffer, 0, len);
  if (!Read_bytes(f, buffer, len))
  {
    GFX2_Log(GFX2_ERROR, "error reading\n");
    free(buffer);
    fclose(f);
    return 0;
  }
  if (!GFX2_is_mem_filled_with(buffer, b, len))
  {
    GFX2_Log(GFX2_ERROR, "byte values mismatch\n");
    free(buffer);
    fclose(f);
    return 0;
  }
  free(buffer);
  if (File_length_file(f) != len)
  {
    GFX2_Log(GFX2_ERROR, "File_length_file() returned %lu (should be %lu)\n",
             File_length_file(f), len);
    fclose(f);
    return 0;
  }
  fclose(f);
  if (File_length(path) != len)
  {
    GFX2_Log(GFX2_ERROR, "File_length() returned %lu (should be %lu)\n",
             File_length(path), len);
    return 0;
  }
  Remove_path(path);
  return 1;
}
