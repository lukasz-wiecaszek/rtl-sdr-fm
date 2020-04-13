# rtl-sdr-fm

RTL SDR FM receiver heavily based on rtl_fm.c from rtlsdr lib.
It uses a little bit of C++ plus complex calculus.
In fact it is very customized.
It outputs only 1 channel (mono) pcm samples (16 bits wide (LE)) at 48kHz.

I use following command
    rtl-sdr-fm -f XXX | aplay -r 48000 -f S16_LE -t raw -c 1
to listen to my fm stations.

