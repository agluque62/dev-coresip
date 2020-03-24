/*/////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2005-2007 Intel Corporation. All Rights Reserved.
//
//     Intel(R) Integrated Performance Primitives
//     USC - Unified Speech Codec interface library
//
// By downloading and installing USC codec, you hereby agree that the
// accompanying Materials are being provided to you under the terms and
// conditions of the End User License Agreement for the Intel(R) Integrated
// Performance Primitives product previously accepted by you. Please refer
// to the file ippEULA.rtf or ippEULA.txt located in the root directory of your Intel(R) IPP
// product installation for more information.
//
// A speech coding standards promoted by ITU, ETSI, 3GPP and other
// organizations. Implementations of these standards, or the standard enabled
// platforms may require licenses from various entities, including
// Intel Corporation.
//
//
// Purpose: AMRWB speech codec: tables.
//
*/

#include "ownamrwb.h"

__ALIGN32 CONST Ipp32s LagWindowTbl[LP_ORDER] =
{
   2146337792, 2143546880, 2138903424, 2132419328,
   2124111616, 2114001792, 2102115840, 2088484096,
   2073141760, 2056127616, 2037485184, 2017261056,
   1995505920, 1972273792, 1947621888, 1921610624
};

__ALIGN32 CONST Ipp16s LPWindowTbl[WINDOW_SIZE] =
{
    2621,    2622,    2626,    2632,    2640,    2650,    2662,    2677,    2694,
    2714,    2735,    2759,    2785,    2814,    2844,    2877,    2912,    2949,
    2989,    3031,    3075,    3121,    3169,    3220,    3273,    3328,    3385,
    3444,    3506,    3569,    3635,    3703,    3773,    3845,    3919,    3996,
    4074,    4155,    4237,    4321,    4408,    4496,    4587,    4680,    4774,
    4870,    4969,    5069,    5171,    5275,    5381,    5489,    5599,    5710,
    5824,    5939,    6056,    6174,    6295,    6417,    6541,    6666,    6793,
    6922,    7052,    7185,    7318,    7453,    7590,    7728,    7868,    8009,
    8152,    8296,    8442,    8589,    8737,    8887,    9038,    9191,    9344,
    9499,    9655,    9813,    9971,   10131,   10292,   10454,   10617,   10781,
   10946,   11113,   11280,   11448,   11617,   11787,   11958,   12130,   12303,
   12476,   12650,   12825,   13001,   13178,   13355,   13533,   13711,   13890,
   14070,   14250,   14431,   14612,   14793,   14975,   15158,   15341,   15524,
   15708,   15891,   16076,   16260,   16445,   16629,   16814,   16999,   17185,
   17370,   17555,   17740,   17926,   18111,   18296,   18481,   18666,   18851,
   19036,   19221,   19405,   19589,   19773,   19956,   20139,   20322,   20504,
   20686,   20867,   21048,   21229,   21408,   21588,   21767,   21945,   22122,
   22299,   22475,   22651,   22825,   22999,   23172,   23344,   23516,   23686,
   23856,   24025,   24192,   24359,   24525,   24689,   24853,   25016,   25177,
   25337,   25496,   25654,   25811,   25967,   26121,   26274,   26426,   26576,
   26725,   26873,   27019,   27164,   27308,   27450,   27590,   27729,   27867,
   28003,   28137,   28270,   28401,   28531,   28659,   28785,   28910,   29033,
   29154,   29274,   29391,   29507,   29622,   29734,   29845,   29953,   30060,
   30165,   30268,   30370,   30469,   30566,   30662,   30755,   30847,   30936,
   31024,   31109,   31193,   31274,   31354,   31431,   31506,   31579,   31651,
   31719,   31786,   31851,   31914,   31974,   32032,   32088,   32142,   32194,
   32243,   32291,   32336,   32379,   32419,   32458,   32494,   32528,   32560,
   32589,   32617,   32642,   32664,   32685,   32703,   32719,   32733,   32744,
   32753,   32760,   32764,   32767,   32767,   32765,   32757,   32745,   32727,
   32705,   32678,   32646,   32609,   32567,   32520,   32468,   32411,   32349,
   32283,   32211,   32135,   32054,   31968,   31877,   31781,   31681,   31575,
   31465,   31351,   31231,   31107,   30978,   30844,   30706,   30563,   30415,
   30263,   30106,   29945,   29779,   29609,   29434,   29255,   29071,   28883,
   28691,   28494,   28293,   28087,   27878,   27664,   27446,   27224,   26997,
   26767,   26533,   26294,   26052,   25806,   25555,   25301,   25043,   24782,
   24516,   24247,   23974,   23698,   23418,   23134,   22847,   22557,   22263,
   21965,   21665,   21361,   21054,   20743,   20430,   20113,   19794,   19471,
   19146,   18817,   18486,   18152,   17815,   17476,   17134,   16789,   16442,
   16092,   15740,   15385,   15028,   14669,   14308,   13944,   13579,   13211,
   12841,   12470,   12096,   11721,   11344,   10965,   10584,   10202,    9819,
    9433,    9047,    8659,    8270,    7879,    7488,    7095,    6701,    6306,
    5910,    5514,    5116,    4718,    4319,    3919,    3519,    3118,    2716,
    2315,    1913,    1510,    1108,     705,     302
};

__ALIGN32 CONST Ipp16s FirUpSamplTbl[120] =
{
       -1,    12,   -33,    68, -119, //k=0
      191,  -291,   430,  -634,  963,
    -1616,  3792, 15317, -2496, 1288,
     -809,   542,  -369,   247, -160,
       96,   -52,    23,    -6,
       -4,    24,   -62,   124, -213, //k=1
      338,  -510,   752, -1111, 1708,
    -2974,  8219, 12368, -3432, 1881,
    -1204,   812,  -552,   368, -235,
      139,   -73,    30,    -7,
       -7,    30,   -73,   139, -235, //k=2
      368,  -552,   812, -1204, 1881,
    -3432, 12368,  8219, -2974, 1708,
    -1111,   752,  -510,   338, -213,
      124,   -62,    24,    -4,
       -6,    23,   -52,    96, -160, //k=3
      247,  -369,   542,  -809, 1288,
    -2496, 15317,  3792, -1616,  963,
     -634,   430,  -291,   191, -119,
       68,   -33,    12,    -1,
        0,     0,     0,     0,    0, //k=4
        0,     0,     0,     0,    0,
        0, 16384,     0,     0,    0,
        0,     0,     0,     0,    0,
        0,     0,     0,     0
};

__ALIGN32 CONST Ipp16s FirDownSamplTbl[120] =
{            /* table x4/5 */
      -1,     0,    18,   -58,//k=0
      99,   -95,     0,   198,
    -441,   601,  -507,     0,
    1030, -2746,  6575, 12254,
       0, -1293,  1366,  -964,
     434,     0,  -233,   270,
    -188,    77,     0,   -26,
      19,    -6,
      -3,     9,     0,   -41,//k=1
     111,  -170,   153,     0,
    -295,   649,  -888,   770,
       0, -1997,  9894,  9894,
   -1997,     0,   770,  -888,
     649,  -295,     0,   153,
    -170,   111,   -41,     0,
       9,    -3,
      -6,    19,   -26,     0,//k=2
      77,  -188,   270,  -233,
       0,   434,  -964,  1366,
   -1293,     0, 12254,  6575,
   -2746,  1030,     0,  -507,
     601,  -441,   198,     0,
     -95,    99,   -58,    18,
       0,    -1,
      -5,    24,   -50,    54,//k=3
       0,  -128,   294,  -408,
     344,     0,  -647,  1505,
   -2379,  3034, 13107,  3034,
   -2379,  1505,  -647,     0,
     344,  -408,   294,  -128,
       0,    54,   -50,    24,
      -5,     0
};

__ALIGN32 CONST Ipp16s IspInitTbl[LP_ORDER] =
{
    32138,  30274,  27246,  23170,
    18205,  12540,   6393,      0,
    -6393, -12540, -18205, -23170,
   -27246, -30274, -32138,   1475
};

__ALIGN32 CONST Ipp16s IsfInitTbl[LP_ORDER] =
{
    1024,  2048,  3072,  4096,
    5120,  6144,  7168,  8192,
    9216, 10240, 11264, 12288,
   13312, 14336, 15360,  3840
};

/* High Band encoding */
__ALIGN32 CONST Ipp16s HPGainTbl[16] =
{
    3624,  4673,  5597,  6479,
    7425,  8378,  9324, 10264,
   11210, 12206, 13391, 14844,
   16770, 19655, 24289, 32728
};

__ALIGN32 CONST Ipp16s Fir6k_7kTbl[31] =
{
      -32,     47,     32,    -27,   -369,    1122,
    -1421,      0,   3798,  -8880,  12349,  -10984,
     3548,   7766, -18001,  22118, -18001,    7766,
     3548, -10984,  12349,  -8880,   3798,       0,
     -1421,  1122,   -369,    -27,     32,      47,
      -32
};

__ALIGN32 CONST Ipp16s Fir7kTbl[31] =
{
      -21,    47,   -89,   146,  -203,   229,
     -177,     0,   335,  -839,  1485, -2211,
     2931, -3542,  3953, 28682,  3953, -3542,
     2931, -2211,  1485,  -839,   335,     0,
     -177,   229,  -203,   146,   -89,    47,
      -21
};

__ALIGN32 CONST Ipp16s InterpolFracTbl[NUMBER_SUBFRAME] = {14746, 26214, 31457, 32767};

__ALIGN32 CONST Ipp16s BCoeffHP50Tbl[3] = {4053, -8106, 4053};
__ALIGN32 CONST Ipp16s ACoeffHP50Tbl[3] = {8192, 16211, -8021};

__ALIGN32 CONST Ipp16s BCoeffHP400Tbl[3] = {915, -1830, 915};
__ALIGN32 CONST Ipp16s ACoeffHP400Tbl[3] = {16384, 29280, -14160};

__ALIGN32 CONST Ipp16s PCoeffDownUnusableTbl[7] = {32767, 31130, 29491, 24576, 7537, 1638, 328};
__ALIGN32 CONST Ipp16s CCoeffDownUnusableTbl[7] = {32767, 16384, 8192, 8192, 8192, 4915, 3277};

__ALIGN32 CONST Ipp16s PCoeffDownUsableTbl[7] = {32767, 32113, 31457, 24576, 7537, 1638, 328};
__ALIGN32 CONST Ipp16s CCoeffDownUsableTbl[7] = {32767, 32113, 32113, 32113, 32113, 32113, 22938};

__ALIGN32 CONST Ipp16s ACoeffTbl[4] = {8192, 21663, -19258, 5734};
__ALIGN32 CONST Ipp16s BCoeffTbl[4] = {-3432, +10280, -10280, +3432};


/* number of parameters per modes (values must be <= MAX_PRM_SIZE!) */
__ALIGN32 CONST Ipp16s NumberPrmsTbl[NUM_OF_MODES] = {
  PRMNO_MR660, PRMNO_MR8850, PRMNO_MR12650, PRMNO_MR14250,
  PRMNO_MR15850, PRMNO_MR18250, PRMNO_MR19850, PRMNO_MR23050,
  PRMNO_MR23850, PRMNO_MRDTX
};

/* parameter sizes (# of bits), one table per rate */
static __ALIGN32 CONST Ipp16s NumberBits660Tbl[PRMNO_MR660] = {
   1,                                       /* VAD             */
   8,  8, 7, 7, 6,                          /* ISFs            */
   8, 12, 6,                                /* first subframe  */
   5, 12, 6,                                /* second subframe */
   5, 12, 6,                                /* third subframe  */
   5, 12, 6,                                /* fourth subframe */
};

static __ALIGN32 CONST Ipp16s NumberBits8850Tbl[PRMNO_MR8850] = {
   1,                                       /* VAD             */
   8, 8, 6, 7, 7, 5, 5,                     /* ISFs            */
   8, 5, 5, 5, 5, 6,                        /* first subframe  */
   5, 5, 5, 5, 5, 6,                        /* second subframe */
   8, 5, 5, 5, 5, 6,                        /* third subframe  */
   5, 5, 5, 5, 5, 6,                        /* fourth subframe */
};

static __ALIGN32 CONST Ipp16s NumberBits12650Tbl[PRMNO_MR12650] = {
   1,                                       /* VAD             */
   8, 8, 6, 7, 7, 5, 5,                     /* ISFs            */
   9, 1, 9, 9, 9, 9, 7,                     /* first subframe  */
   6, 1, 9, 9, 9, 9, 7,                     /* second subframe */
   9, 1, 9, 9, 9, 9, 7,                     /* third subframe  */
   6, 1, 9, 9, 9, 9, 7,                     /* fourth subframe */
};

static __ALIGN32 CONST Ipp16s NumberBits14250Tbl[PRMNO_MR14250] = {
   1,                                       /* VAD             */
   8, 8,  6,  7, 7, 5, 5,                   /* ISFs            */
   9, 1, 13, 13, 9, 9, 7,                   /* first subframe  */
   6, 1, 13, 13, 9, 9, 7,                   /* second subframe */
   9, 1, 13, 13, 9, 9, 7,                   /* third subframe  */
   6, 1, 13, 13, 9, 9, 7,                   /* fourth subframe */
};

static __ALIGN32 CONST Ipp16s NumberBits15850Tbl[PRMNO_MR15850] = {
   1,                                       /* VAD             */
   8, 8,  6,  7,  7,  5, 5,                 /* ISFs            */
   9, 1, 13, 13, 13, 13, 7,                 /* first subframe  */
   6, 1, 13, 13, 13, 13, 7,                 /* second subframe */
   9, 1, 13, 13, 13, 13, 7,                 /* third subframe  */
   6, 1, 13, 13, 13, 13, 7,                 /* fourth subframe */
};

static __ALIGN32 CONST Ipp16s NumberBits18250Tbl[PRMNO_MR18250] = {
   1,                                       /* VAD             */
   8, 8,  6,  7,  7,  5, 5,                 /* ISFs            */
   9, 1, 2, 2, 2, 2, 14, 14, 14, 14, 7,     /* first subframe  */
   6, 1, 2, 2, 2, 2, 14, 14, 14, 14, 7,     /* second subframe */
   9, 1, 2, 2, 2, 2, 14, 14, 14, 14, 7,     /* third subframe  */
   6, 1, 2, 2, 2, 2, 14, 14, 14, 14, 7,     /* fourth subframe */
};

static __ALIGN32 CONST Ipp16s NumberBits19850Tbl[PRMNO_MR19850] = {
   1,                                       /* VAD             */
   8, 8,  6,  7,  7,  5, 5,                 /* ISFs            */
   9, 1, 10, 10, 2, 2, 10, 10, 14, 14, 7,   /* first subframe  */
   6, 1, 10, 10, 2, 2, 10, 10, 14, 14, 7,   /* second subframe */
   9, 1, 10, 10, 2, 2, 10, 10, 14, 14, 7,   /* third subframe  */
   6, 1, 10, 10, 2, 2, 10, 10, 14, 14, 7,   /* fourth subframe */
};

static __ALIGN32 CONST Ipp16s NumberBits23050Tbl[PRMNO_MR23050] = {
   1,                                       /* VAD             */
   8, 8,  6,  7,  7,  5, 5,                 /* ISFs            */
   9, 1, 11, 11, 11, 11, 11, 11, 11, 11, 7, /* first subframe  */
   6, 1, 11, 11, 11, 11, 11, 11, 11, 11, 7, /* second subframe */
   9, 1, 11, 11, 11, 11, 11, 11, 11, 11, 7, /* third subframe  */
   6, 1, 11, 11, 11, 11, 11, 11, 11, 11, 7, /* fourth subframe */
};

static __ALIGN32 CONST Ipp16s NumberBits23850Tbl[PRMNO_MR23850] = {
   1,                                          /* VAD             */
   8, 8,  6,  7,  7,  5, 5,                    /* ISFs            */
   9, 1, 11, 11, 11, 11, 11, 11, 11, 11, 7, 4, /* first subframe  */
   6, 1, 11, 11, 11, 11, 11, 11, 11, 11, 7, 4, /* second subframe */
   9, 1, 11, 11, 11, 11, 11, 11, 11, 11, 7, 4, /* third subframe  */
   6, 1, 11, 11, 11, 11, 11, 11, 11, 11, 7, 4, /* fourth subframe */
};

static __ALIGN32 CONST Ipp16s NumberBitsDTXTbl[PRMNO_MRDTX] = {
   6, 6,  6,  5, 5,                            /* ISFs            */
   6,                                          /* log_en_index    */
   1,                                          /* CN_dith         */
};

/* overall table with all parameter sizes for all modes */
__ALIGN32 CONST Ipp16s *pNumberBitsTbl[NUM_OF_MODES] = {
   NumberBits660Tbl,   NumberBits8850Tbl,   NumberBits12650Tbl,   NumberBits14250Tbl,
   NumberBits15850Tbl, NumberBits18250Tbl,  NumberBits19850Tbl,   NumberBits23050Tbl,
   NumberBits23850Tbl, NumberBitsDTXTbl
};

/* HiFreq gain ramp up/down table for WB <-> WB+ switching */
__ALIGN32 CONST Ipp16s tblGainRampHF[64] = {
  504, 1008, 1512, 2016, 2521, 3025, 3529, 4033,
  4537, 5041, 5545, 6049, 6554, 7058, 7562, 8066,
  8570, 9074, 9578, 10082, 10587, 11091, 11595, 12099,
  12603, 13107, 13611, 14115, 14620, 15124, 15628, 16132,
  16636, 17140, 17644, 18148, 18653, 19157, 19661, 20165,
  20669, 21173, 21677, 22181, 22686, 23190, 23694, 24198,
  24702, 25206, 25710, 26214, 26719, 27223, 27727, 28231,
  28735, 29239, 29743, 30247, 30752, 31256, 31760, 32264
};
