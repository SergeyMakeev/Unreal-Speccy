#pragma once

struct RSM_DATA
{
#ifdef MOD_SSE2
    typedef __m128i __m_vec;
#else
    typedef __m64 __m_vec;
#endif // MOD_SSE2
   const __m_vec *colortab; // tables for current frame

   enum { MAX_MIX_FRAMES = 8, FRAME_TABLES_SIZE = MAX_MIX_FRAMES*0x100*4*8 };
   enum { MAX_LINE_DWORDS = MAX_WIDTH };
   unsigned mix_frames, frame_table_size, line_size_d;
   union {
       __m_vec line_buffer[MAX_LINE_DWORDS/(sizeof(__m_vec)/sizeof(DWORD))];
      unsigned line_buffer_d[MAX_LINE_DWORDS];
   };

   unsigned period, frame, rbuf_dst, bias;

   unsigned char *data;
   unsigned char *needframes; // new spectrum frames in next pc frame
   __m_vec *tables;

   RSM_DATA() { data = nullptr; }
   ~RSM_DATA() { free(data); }

   void prepare_line_8(unsigned char *src0);
   void prepare_line_16(unsigned char *src0);
   void prepare_line_32(unsigned char *src0);

   int mode;
   // 0 - 16/32bit RGB. tables:  4*attr+2pix => 2 pixels in MMX QWORD
   // 1 - 16bit YUY2.   tables: 16*attr+4pix => 4 pixels in MMX QWORD
   // 2 - 8bit.         tables: 16*attr+4pix => 4 pixels in DWORD

};

extern RSM_DATA rsm;

void __fastcall render_rsm(unsigned char *dst, unsigned pitch);
void calc_rsm_tables();
