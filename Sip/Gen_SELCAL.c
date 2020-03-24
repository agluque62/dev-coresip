/*
      * annex10.c
      *
      * (c) 2002-01-30, Ralf D. Kloth <ralf at qrq.de>, QRQ.software
      * Feedback welcome.
      *
      * This program generates aero SELCAL tones according to specs of:
      * ICAO: AERONAUTICAL TELECOMMUNICATIONS, Annex 10 to the
      *       Convention on International Civil Aviation, Volume I,
      *       4th edition of 1985 (amended 1987).
      * see http://www.kloth.net/software/annex10.php for details
      *
      * Written and tested on Linux 2.4.
      *    usage: annex10 <SELCAL>
      *    example: annex10 AGCQ   
      * Compiles on Linux 2.4 with
      *    $ gcc -o annex10 annex10.c
      * and needs a PC standard soundcard 
      *
      * This program is free software; you can redistribute it and/or modify it.
      * THIS PRGOGRAM IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESSED OR IMPLIED
      * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
      * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
      * You are using this program at own risk.
      *
      * Credits:
      * the soundcard driving functions {mysine, two_tones, silence} 
      * have been taken from:
      * Phrack, Volume Seven, Issue Fifty, 13 of 16, 1997-04-09:
      * DTMF Encoding and Decoding In C, by Mr. Blue 
      * http://www.phrack.org/show.php?p=50&a=13
      * released under the "phrack copynow license".
      */
     
     #include <fcntl.h>
     #include <stdio.h> 
     #include <stdlib.h>
     #include <string.h>    
     #include <unistd.h>   
     #include <errno.h>
     #include <sys/soundcard.h>
     
     #define FSAMPLE 8000   /* sampling rate, 8 kHz */
     #define FLOAT_TO_SAMPLE(x) ((char)((x + 1.0) * 127.0))
     #define SOUND_DEV  "/dev/dsp"
     
     typedef char sample;
     
     /*
      * take the sine of x, where x is 0 to 65535 (for 0 to 360 degrees)
      */
     float mysine(in)
     short in;
     {
       static coef[] = {
          3.140625, 0.02026367, -5.325196, 0.5446778, 1.800293 };
       float x, y, res;
       int sign, i;
      
       if(in < 0) {       /* force positive */
         sign = -1;
         in = -in;
       } else
         sign = 1;
       if(in >= 0x4000)      /* 90 degrees */
         in = 0x8000 - in;   /* 180 degrees - in */
       x = in * (1/32768.0); 
       y = x;                /* y holds x^i) */
       res = 0;
       for(i=0; i<5; i++) {
         res += y * coef[i];
         y *= x;
       }
       return(res * sign); 
     }
     
     /*
      * play tone1 and tone2 (in Hz)
      * for 'length' milliseconds
      * outputs samples to sound_out
      */
     two_tones(sound_out,tone1,tone2,length)
     int sound_out;
     unsigned int tone1, tone2, length;
     {
     #define BLEN 128
       sample cout[BLEN];
       float out;
       unsigned int ad1, ad2;
       short c1, c2;
       int i, l, x;
        
       ad1 = (tone1 << 16) / FSAMPLE;
       ad2 = (tone2 << 16) / FSAMPLE;
       l = (length * FSAMPLE) / 1000;
       x = 0;
       for( c1=0, c2=0, i=0 ;
            i < l;
            i++, c1+= ad1, c2+= ad2 ) {
         out = (mysine(c1) + mysine(c2)) * 0.5;
         cout[x++] = FLOAT_TO_SAMPLE(out);
         if (x == BLEN) {
           write(sound_out, cout, x * sizeof(sample));
           x = 0;
         }
       }
       write(sound_out, cout, x);
     }
     
     /*
      * silence on 'sound_out'
      * for 'length' milliseconds
      */
     silence(sound_out,length)
     int sound_out;
     unsigned int length;
     {
       int l, i, x;
       static sample c0 = FLOAT_TO_SAMPLE(0.0);
       sample cout[BLEN];
     
       x = 0;
       l = (length * FSAMPLE) / 1000;
       for(i=0; i < l; i++) {
         cout[x++] = c0;
         if (x == BLEN) {
           write(sound_out, cout, x * sizeof(sample));
           x = 0;
         }
       }
       write(sound_out, cout, x);
     }
     
     /*
      * play two tones
      * for 'length' milliseconds
      */
     selcall(sound_out, digit1, digit2, length)
     int sound_out, digit1, digit2, length;
     {
       /* Tones from the Annex10 red series */
       static int red[] = {
       /*  A      B      C      D      E      F      G      H   */
         312.6, 346.7, 384.6, 426.6, 473.2, 524.8, 582.1, 645.7, 
       /*  J      K      L      M       P       Q       R       S */
         716.1, 794.3, 881.0, 977.2, 1083.9, 1202.3, 1333.5, 1479.1 };
     
       two_tones(sound_out, red[digit1], red[digit2], length);
     }
     
     /* 
      * --- main -----------------------------------------------------------
      */
     int main(int argc, char *argv[])
     {
       int sound_out;
       int i, j;
       int debug = 0;
       int inpt[4];
       char inpts[5] = "    \0";
     
     /* get command line args */
       if (argc < 2) {
         printf("Usage: annex10 <SELCAL>\n");
         exit(1);
       }
     
       /* check the input to be in [A..H, J..M, P..S] and translate to [0..15] */
       for (i = 0; i <= 3; i++) {
         inpts[i] = toupper(argv[1][i]);
         j = inpts[i] -65;
         if ((j < 0) || (j > 18) || (j == 8) || (j == 13) || (j == 14)) {
           printf("%s is not a valid SELCAL\n",argv[1]);
           exit(1);              /* not in [A..H, J..M, P..S] */
         }
         if (j > 12) j = j - 2;  /* N and O left out */
         if (j > 7)  j = j - 1;  /* I left out */
         inpt[i] = j;
       }
       /* ignore invalid selcal combinations like BA.. */
       for (i = 1; i <= 3; i = i+2) {
         if (inpt[i] <= inpt[i-1]) {
           printf("%s is not a valid SELCAL\n",inpts);
           exit(1);
         }
       }
       /* ignore invalid selcal combinations like ABAB */
       if ((inpt[0] == inpt[2]) && (inpt[1] == inpt[3])) {
           printf("%s is not a valid SELCAL\n",inpts);
           exit(1);
         }
     
       printf("SELCALling %s",inpts);
       if (debug) {
         printf(" [");
         for (i = 0; i <= 3; i++) {
           if (i == 2) printf(" -");
           printf(" %d",inpt[i]);
         }
         printf(" ]");
       }
       printf("\n");
     
     /* open sound device */
       sound_out = open(SOUND_DEV,O_RDWR);
       if(sound_out < 0) {
         printf("Error opening sound device\n");
         exit(1);
       }
     
     /* send the selcal */
       selcall(sound_out,inpt[0],inpt[1],1000);
       silence(sound_out,200);
       selcall(sound_out,inpt[2],inpt[3],1000);
     
     /* close sound device */
       close(sound_out);
     }
     
     /* end */
