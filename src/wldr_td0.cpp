#include "std.h"

#include "emul.h"
#include "vars.h"
#include "wd93crc.h"

#include "util.h"

int FDD::write_td0(FILE *ff)
{
   unsigned char zerosec[256] = { 0 };
   unsigned char td0hdr[12] = { 0 };

   *(unsigned short*)td0hdr = WORD2('T','D');
   td0hdr[4] = 21; td0hdr[6] = 2; td0hdr[9] = (unsigned char)sides;
   if (*dsc) td0hdr[7] = 0x80;
   *(unsigned short*)(td0hdr + 10) = crc16(td0hdr, 10);
   fwrite(td0hdr, 1, 12, ff);
   if (*dsc) {
      unsigned char inf[0x200] = { 0 };
      strcpy((char*)inf+10, dsc);
      unsigned len = unsigned(strlen(dsc)+1);
      *(unsigned*)(inf+2) = len;
      *(unsigned short*)inf = crc16(inf+2, len+8);
      fwrite(inf, 1, len+10, ff);
   }

   unsigned c; //Alone Coder 0.36.7
   for (/*unsigned*/ c = 0; c < cyls; c++)
      for (unsigned s = 0; s < sides; s++) {
         t.seek(this,c,s,LOAD_SECTORS);
         unsigned char bf[16];
         *bf = u8(t.s);
         bf[1] = u8(c); bf[2] = u8(s);
         bf[3] = (unsigned char)crc16(bf, 3);
         fwrite(bf, 1, 4, ff);
         for (unsigned sec = 0; sec < t.s; sec++) {
             if(!t.hdr[sec].data)
             {
                 t.hdr[sec].data = zerosec;
                 t.hdr[sec].datlen = 256;
                 t.hdr[sec].l = 1;
             }
            *(unsigned*)bf = *(unsigned*)&t.hdr[sec];
            bf[4] = 0; // flags
            bf[5] = (unsigned char)crc16(t.hdr[sec].data, t.hdr[sec].datlen);
            *(unsigned short*)(bf+6) = u16(t.hdr[sec].datlen + 1);
            bf[8] = 0; // compression type = none
            fwrite(bf, 1, 9, ff);
            if (fwrite(t.hdr[sec].data, 1, t.hdr[sec].datlen, ff) != t.hdr[sec].datlen) return 0;
         }
      }
   c = WORD4(0xFF,0,0,0);
   if (fwrite(&c, 1, 4, ff) != 4) return 0;
   return 1;
}


unsigned unpack_lzh(unsigned char *src, unsigned size, unsigned char *buf);

// No ID address field was present for this sector,
// but there is a data field. The sector information in
// the header represents fabricated information.
const ULONG TD0_SEC_NO_ID = 0x40;

// This sector's data field is missing; no sector data follows this header.
const ULONG TD0_SEC_NO_DATA = 0x20;

// A DOS sector copy was requested; this sector was not allocated.
// In this case, no sector data follows this header.
const ULONG TD0_SEC_NO_DATA2 = 0x10;

#pragma pack(push, 1)
struct TTd0Sec
{
    u8 c;
    u8 h;
    u8 s;
    u8 n;
    u8 flags;
    u8 crc;
};
#pragma pack(pop)

int FDD::read_td0()
{
   if (*(short*)snbuf == WORD2('t','d'))
   { // packed disk
      unsigned char *tmp = (unsigned char*)malloc(snapsize);
      memcpy(tmp, snbuf+12, snapsize-12);
      snapsize = 12+unpack_lzh(tmp, snapsize-12, snbuf+12);
      ::free(tmp);
      //*(short*)snbuf = WORD2('T','D');
   }

   char dscbuffer[sizeof(dsc)];
   *dscbuffer = 0;

   unsigned char *start = snbuf+12;
   if (snbuf[7] & 0x80) // coment record
   {
      start += 10;
      unsigned len = *(unsigned short*)(snbuf+14);
      start += len;
      if (len >= sizeof dsc)
          len = sizeof(dsc)-1;
      memcpy(dscbuffer, snbuf+12+10, len);
      dscbuffer[len] = 0;
   }
   unsigned char *td0_src = start;

   unsigned sides = (snbuf[9] == 1 ? 1 : 2);
   unsigned max_cyl = 0;

   for (;;)
   {
      unsigned char s = *td0_src; // Sectors
      if (s == 0xFF)
          break;
      max_cyl = max(max_cyl, unsigned(td0_src[1])); // PhysTrack
      td0_src += 4; // sizeof(track_rec)
      for (; s; s--)
      {
         unsigned char flags = td0_src[4];
         td0_src += 6; // sizeof(sec_rec)

         assert(td0_src <= snbuf + snapsize);

         if (td0_src > snbuf + snapsize)
             return 0;
         td0_src += *(unsigned short*)td0_src + 2; // data_len
      }
   }

   if(max_cyl+1 > MAX_CYLS)
   {
       err_printf("cylinders (%d) > MAX_CYLS(%d)", max_cyl, MAX_CYLS);
       return 0;
   }

   newdisk(max_cyl+1, sides);
   memcpy(dsc, dscbuffer, sizeof dsc);

   td0_src = start;
   for (;;)
   {
      unsigned char t0[16384];
      unsigned char *dst = t0;
      unsigned char *trkh = td0_src;
      td0_src += 4; // sizeof(track_rec)

      if(*trkh == 0xFF)
          break;

      t.seek(this, trkh[1], trkh[2], JUST_SEEK);

      unsigned s = 0;
      for (unsigned se = 0; se < trkh[0]; se++)
      {
         TTd0Sec *SecHdr = (TTd0Sec *)td0_src;
         unsigned sec_size = 128U << (SecHdr->n & 3); // [vv]
         unsigned char flags = SecHdr->flags;
//         printf("fl=%x\n", flags);
//         printf("c=%d, h=%d, s=%d, n=%d\n", SecHdr->c, SecHdr->h, SecHdr->s, SecHdr->n);
         if(flags & (TD0_SEC_NO_ID | TD0_SEC_NO_DATA | TD0_SEC_NO_DATA2)) // skip sectors with no data & sectors without headers
         {
             td0_src += sizeof(TTd0Sec); // sizeof(sec_rec)

             unsigned src_size = *(unsigned short*)td0_src;
//             printf("sz=%d\n", src_size);
             td0_src += 2; // data_len
             unsigned char *end_packed_data = td0_src + src_size;
/*
             u8 method = *td0_src++;
             printf("m=%d\n", method);
             switch(method)
             {
             case 0:
                 {
                     char name[MAX_PATH];
                     sprintf(name, "%02d-%d-%03d-%d.trk", SecHdr->c, SecHdr->h, SecHdr->s, SecHdr->n);
                     FILE *f = fopen(name, "wb");
                     fwrite(td0_src, 1, src_size - 1, f);
                     fclose(f);
                     break;
                 }
             case 1:
                 {
                     unsigned n = *(unsigned short*)td0_src;
                     td0_src += 2;
                     unsigned short data = *(unsigned short*)td0_src;
                     printf("len=%d, data=%04X\n", n, data);
                     break;
                 }
             }
*/
             td0_src = end_packed_data;
             continue;
         }

          // c, h, s, n
         t.hdr[s].c = SecHdr->c;
         t.hdr[s].s = SecHdr->h;
         t.hdr[s].n = SecHdr->s;
         t.hdr[s].l = SecHdr->n;
         t.hdr[s].c1 = t.hdr[s].c2 = 0;
         t.hdr[s].data = dst;
         t.hdr[s].datlen = 0;

         td0_src += sizeof(TTd0Sec); // sizeof(sec_rec)

         unsigned src_size = *(unsigned short*)td0_src;
         td0_src += 2; // data_len
         unsigned char *end_packed_data = td0_src + src_size;

         if(src_size == 0)
         {
             printf("sector data size is zero\n");
             goto shit;
         }

         if(src_size > sec_size + 1)
         {
             printf("sector overflow: src_size=%u > (sec_size+1)=%u\n", src_size, sec_size + 1);
             goto shit;
         }

         memset(dst, 0, sec_size);

         switch (*td0_src++) // Method
         {
            case 0:  // raw sector
               memcpy(dst, td0_src, src_size-1);
               break;
            case 1:  // repeated 2-byte pattern
            {
               unsigned n = *(unsigned short*)td0_src;
               td0_src += 2;
               unsigned short data = *(unsigned short*)td0_src;
               for (unsigned i = 0; i < n; i++)
                  *(unsigned short*)(dst+2*i) = data;
               break;
            }
            case 2: // RLE block
            {
               u8 n;
               unsigned char *d0 = dst;
               do
               {
                  u8 RleData[510];
                  u8 l = 2 * (*td0_src++);
                  if(l == 0) // Zero count means a literal data block
                  {
                      n = *td0_src++;
                      if(dst + n > d0 + sec_size)
                      {
                          printf("sector overflow: pos=0x%x, l=%u, n=%u, sec_size=%u, src_size=%u\n",
                              unsigned(td0_src - 2 - snbuf), unsigned(l), unsigned(n), sec_size, src_size);
                          goto shit;
                      }
                      memcpy(dst, td0_src, n);
                      td0_src += n;
                      dst += n;
                  }
                  else // repeated fragment
                  {
                      n = *td0_src++;
                      memcpy(RleData, td0_src, l);
                      td0_src += l;
                      for ( ; n; n--)
                      {
                          if(dst + l > d0 + sec_size)
                          {
                              printf("sector overflow: pos=0x%x, dpos=0x%x, l=%u, sec_size=%u, src_size=%u\n",
                                  unsigned(td0_src - 2 - snbuf), unsigned((dst + l) - d0), unsigned(l), sec_size, src_size);
                              goto shit;
                          }
                          memcpy(dst, RleData, l);
                          dst += l;
                      }
                  }
               } while (td0_src < end_packed_data);
               dst = d0;
               break;
            }
            default: // error!
            errmsg("unknown block type");

            shit:
               errmsg("bad TD0 file");
               return 0;
         }
         dst += sec_size;
         td0_src = end_packed_data;
         s++;
      }
      t.s = s;
      t.format();
   }
   return 1;
}

// ------------------------------------------------------ LZH unpacker


static unsigned char *packed_ptr, *packed_end;

static unsigned readChar()
{
    if(packed_ptr < packed_end)
    {
        return *packed_ptr++;
    }
    else
    {
        return -1U;
    }
}

static unsigned char d_code[256] =
{
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
        0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
        0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
        0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
        0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A,
        0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B,
        0x0C, 0x0C, 0x0C, 0x0C, 0x0D, 0x0D, 0x0D, 0x0D,
        0x0E, 0x0E, 0x0E, 0x0E, 0x0F, 0x0F, 0x0F, 0x0F,
        0x10, 0x10, 0x10, 0x10, 0x11, 0x11, 0x11, 0x11,
        0x12, 0x12, 0x12, 0x12, 0x13, 0x13, 0x13, 0x13,
        0x14, 0x14, 0x14, 0x14, 0x15, 0x15, 0x15, 0x15,
        0x16, 0x16, 0x16, 0x16, 0x17, 0x17, 0x17, 0x17,
        0x18, 0x18, 0x19, 0x19, 0x1A, 0x1A, 0x1B, 0x1B,
        0x1C, 0x1C, 0x1D, 0x1D, 0x1E, 0x1E, 0x1F, 0x1F,
        0x20, 0x20, 0x21, 0x21, 0x22, 0x22, 0x23, 0x23,
        0x24, 0x24, 0x25, 0x25, 0x26, 0x26, 0x27, 0x27,
        0x28, 0x28, 0x29, 0x29, 0x2A, 0x2A, 0x2B, 0x2B,
        0x2C, 0x2C, 0x2D, 0x2D, 0x2E, 0x2E, 0x2F, 0x2F,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
};

static unsigned char d_len[256] =
{
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
        0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
        0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
        0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
        0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
        0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
        0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
        0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
        0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
        0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
        0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
        0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
        0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
};


const int N = 4096;     // buffer size
const int F = 60;       // lookahead buffer size
const int THRESHOLD =   2;
const int NIL = N;      // leaf of tree

static unsigned char text_buf[N + F - 1];

const int N_CHAR = (256 - THRESHOLD + F);       // kinds of characters (character code = 0..N_CHAR-1)
const int T =   (N_CHAR * 2 - 1);       // size of table
const int R = (T - 1);                  // position of root
const int MAX_FREQ = 0x8000;            // updates tree when the
                                    // root frequency comes to this value.

static unsigned short freq[T + 1];        // frequency table

static short prnt[T + N_CHAR]; // pointers to parent nodes, except for the
                        // elements [T..T + N_CHAR - 1] which are used to get
                        // the positions of leaves corresponding to the codes.
static short son[T];           // pointers to child nodes (son[], son[] + 1)


static int r;

static unsigned getbuf;
static unsigned char getlen;

static int GetBit()      /* get one bit */
{
  unsigned i;

  while (getlen <= 8)
  {
      if((i = readChar()) == -1U)
      {
          i = 0;
      }
      getbuf |= unsigned(i << (8 - getlen));
      getlen += 8;
  }
  i = getbuf;
  getbuf <<= 1;
  getlen--;
  return ((i>>15) & 1);
}

static int GetByte()     /* get one byte */
{
  unsigned i;

  while (getlen <= 8)
  {
      if((i = readChar()) == -1U)
      {
          i = 0;
      }
      getbuf |= unsigned(i << (8 - getlen));
      getlen += 8;
  }
  i = getbuf;
  getbuf <<= 8;
  getlen -= 8;
  return (i >> 8) & 0xFF;
}

static void StartHuff()
{
  int i, j;

  getbuf = 0; getlen = 0;
  for (i = 0; i < N_CHAR; i++) {
    freq[i] = 1;
    son[i] = i16(i + T);
    prnt[i + T] = i16(i);
  }
  i = 0; j = N_CHAR;
  while (j <= R) {
    freq[j] = freq[i] + freq[i + 1];
    son[j] = i16(i);
    prnt[i] = prnt[i + 1] = i16(j);
    i += 2; j++;
  }
  freq[T] = 0xffff;
  prnt[R] = 0;

  for (i = 0; i < N - F; i++) text_buf[i] = ' ';
  r = N - F;
}

/* reconstruction of tree */
static void reconst()
{
  int i, j, k;
  int f;

  /* collect leaf nodes in the first half of the table */
  /* and replace the freq by (freq + 1) / 2. */
  j = 0;
  for(i = 0; i < T; i++)
  {
    if(son[i] >= T)
    {
      freq[j] = (freq[i] + 1) / 2;
      son[j] = son[i];
      j++;
    }
  }
  /* begin constructing tree by connecting sons */
  for(i = 0, j = N_CHAR; j < T; i += 2, j++)
  {
    k = i + 1;
    f = freq[j] = freq[i] + freq[k];
    for(k = j - 1; f < freq[k]; k--);
    k++;
    size_t l = unsigned(j - k) * sizeof(*freq);
    MoveMemory(&freq[k + 1], &freq[k], l);
    freq[k] = u16(f);
    MoveMemory(&son[k + 1], &son[k], l);
    son[k] = i16(i);
  }
  /* connect prnt */
  for (i = 0; i < T; i++)
    if ((k = son[i]) >= T) prnt[k] = i16(i);
    else prnt[k] = prnt[k + 1] = i16(i);
}


/* increment frequency of given code by one, and update tree */

static void update(int c)
{
  int i, j, k, l;

  if(freq[R] == MAX_FREQ) reconst();

  c = prnt[c + T];
  do {
    k = ++freq[c];

    /* if the order is disturbed, exchange nodes */
    if (k > freq[l = c + 1])
    {
      while (k > freq[++l]);
      l--;
      freq[c] = freq[l];
      freq[l] = u16(k);

      i = son[c];
      prnt[i] = i16(l);
      if (i < T) prnt[i + 1] = i16(l);

      j = son[l];
      son[l] = i16(i);

      prnt[j] = i16(c);
      if (j < T) prnt[j + 1] = i16(c);
      son[c] = i16(j);

      c = l;
    }
  } while ((c = prnt[c]) != 0);  /* repeat up to root */
}

static int DecodeChar()
{
  int c;

  c = son[R];

  /* travel from root to leaf, */
  /* choosing the smaller child node (son[]) if the read bit is 0, */
  /* the bigger (son[]+1} if 1 */
  while(c < T) c = son[c + GetBit()];
  c -= T;
  update(c);
  return c;
}

static int DecodePosition()
{
  int i, j, c;

  /* recover upper 6 bits from table */
  i = GetByte();
  c = (int)d_code[i] << 6;
  j = d_len[i];
  /* read lower 6 bits verbatim */
  j -= 2;
  while (j--) i = (i << 1) + GetBit();
  return c | (i & 0x3f);
}

unsigned unpack_lzh(unsigned char *src, unsigned size, unsigned char *buf)
{
  packed_ptr = src; packed_end = src+size;
  int  i, j, k, c;
  unsigned count = 0;
  StartHuff();

//  while (count < textsize)  // textsize - sizeof unpacked data
  while (packed_ptr < packed_end)
  {
    c = DecodeChar();
    if(c < 256)
    {
      *buf++ = u8(c);
      text_buf[r++] = u8(c);
      r &= (N - 1);
      count++;
    } else {
      i = (r - DecodePosition() - 1) & (N - 1);
      j = c - 255 + THRESHOLD;
      for (k = 0; k < j; k++)
      {
        c = text_buf[(i + k) & (N - 1)];
        *buf++ = u8(c);
        text_buf[r++] = u8(c);
        r &= (N - 1);
        count++;
      }
    }
  }
  return count;
}
