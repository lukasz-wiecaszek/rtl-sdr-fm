/**
 * @file rtl-sdr-fm.cpp
 *
 * RTL SDR FM receiver heavily based on rtl_fm.c from rtlsdr lib.
 * It uses a little bit of C++ plus complex calculus.
 * In fact it is very customized.
 * It outputs only 1 channel (mono) pcm samples (16 bits wide (LE)) at 48kHz.
 * I use
 *    rtl-sdr-fm -f XXX | aplay -r 48000 -f S16_LE -t raw -c 1
 * to listen to my fm stations.
 *
 * TODO:
 * - get rid of floating points operations
 *
 * @author Lukasz Wiecaszek <lukasz.wiecaszek@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 */

/*===========================================================================*\
 * system header files
\*===========================================================================*/
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <math.h>

#include <vector>
#include <chrono>
#include <thread>

#include <rtl-sdr.h>

/*===========================================================================*\
 * project header files
\*===========================================================================*/
#include "strtointeger.hpp"
#include "power_of_two.hpp"
#include "fixq15.hpp"
#include "complex.hpp"
#include "pipeline.hpp"
#include "ringbuffer.hpp"

/*===========================================================================*\
 * preprocessor #define constants and macros
\*===========================================================================*/
#define kHz                 *1000

#define IQBUF_SIZE           (16 * 1024 * 2)
#define IDLE_LOOPS_NUM       (1)
#define AUDIO_SAMPLE_RATE    (48 kHz)
#define OVERSAMPLING_1       (4)
#define IF_SAMPLE_RATE       (AUDIO_SAMPLE_RATE * OVERSAMPLING_1)
#define OVERSAMPLING_2       (6)
#define RTL_SDR_SAMPLE_RATE  (IF_SAMPLE_RATE * OVERSAMPLING_2)

/*===========================================================================*\
 * local type definitions
\*===========================================================================*/
template<typename T>
struct buffer : public ymn::pipeline::buffer
{
    explicit buffer() :
        ymn::pipeline::buffer{},
        vector()
    {
    }

    explicit buffer(std::size_t size) :
        ymn::pipeline::buffer{},
        vector(size)
    {
    }

    std::vector<T> vector;
};

using iq_t = ymn::complex<ymn::fixq15>;
using iq_buffer_uptr = std::unique_ptr<buffer<iq_t>>;

iq_buffer_uptr to_iq_buffer_uptr(ymn::pipeline::buffer_uptr&& p)
{
    return iq_buffer_uptr{static_cast<buffer<iq_t>*>(p.release())};
}

using pcm_t = int16_t;
using pcm_buffer_uptr = std::unique_ptr<buffer<pcm_t>>;

pcm_buffer_uptr to_pcm_buffer_uptr(ymn::pipeline::buffer_uptr&& p)
{
    return pcm_buffer_uptr{static_cast<buffer<pcm_t>*>(p.release())};
}

/*===========================================================================*\
 * global object definitions
\*===========================================================================*/

/*===========================================================================*\
 * local function declarations
\*===========================================================================*/
static void print_usage(const char* progname);
static void signal_handler(int signum);
static void install_signal_handler(void);
static void fm_demod(pcm_t *pcmbuf, iq_t* iqbuf, const std::size_t N);
static int verbose_device_search(const char *s);

/*===========================================================================*\
 * local object definitions
\*===========================================================================*/
static rtlsdr_dev_t *rtlsdr_device = NULL;
static uint8_t iqbuf_u8[IQBUF_SIZE];
static std::unique_ptr<ymn::pipeline> pipeline;

/*===========================================================================*\
 * inline function definitions
\*===========================================================================*/
template<std::size_t N>
static inline void polar_rotate_90(uint8_t (&data)[N])
{
    /* 90 rotation is 1+0j, 0+1j, -1+0j, 0-1j or [0, 1, -3, 2, -4, -5, 7, -6] */
    static_assert((N % 8) == 0, "N must be multiple of 8");

    uint8_t tmp;

    for (std::size_t n = 0; n < N; n += 8) {
        tmp = ~data[n + 3];
        data[n + 3] = data[n + 2];
        data[n + 2] = tmp;

        data[n + 4] = ~data[n + 4];
        data[n + 5] = ~data[n + 5];

        tmp = ~data[n + 6];
        data[n + 6] = data[n + 7];
        data[n + 7] = tmp;
    }
}

static inline pcm_t polar_discriminator(iq_t a, iq_t b)
{
    iq_t c = a * b.conj();
    double angle = atan2(c.imag().value(), c.real().value());
    return static_cast<pcm_t>((angle / M_PI) * Q15);
}

static inline iq_buffer_uptr get_iq_buffer_uptr(ymn::iringbuffer<ymn::pipeline::buffer_uptr>* irb)
{
    ymn::pipeline::buffer_uptr buf_uptr;

    long read_status = irb->read(std::move(buf_uptr));
    if (read_status != 1)
        return nullptr;

    return to_iq_buffer_uptr(std::move(buf_uptr));
}

static inline pcm_buffer_uptr get_pcm_buffer_uptr(ymn::iringbuffer<ymn::pipeline::buffer_uptr>* irb)
{
    ymn::pipeline::buffer_uptr buf_uptr;

    long read_status = irb->read(std::move(buf_uptr));
    if (read_status != 1)
        return nullptr;

    return to_pcm_buffer_uptr(std::move(buf_uptr));
}

static void low_pass_filter(std::vector<iq_t>& samples, const std::size_t decimation)
{
    std::size_t n = 0;
    std::size_t i = 0;
    iq_t sum{0, 0};

    const std::size_t N = samples.size();
    iq_t* buf = samples.data();

    while (n < N) {
        sum += buf[n];
        if ((++n % decimation) == 0) {
            buf[i++] = sum;
            sum = iq_t{0, 0};
        }
    }

    samples.resize(N / decimation);
}

static void low_pass_filter(std::vector<pcm_t>& samples, const std::size_t decimation)
{
    std::size_t n = 0;
    std::size_t i = 0;
    long sum = 0;

    const std::size_t N = samples.size();
    pcm_t* buf = samples.data();

    while (n < N) {
        sum += buf[n];
        if ((++n % decimation) == 0) {
            buf[i++] = sum / decimation;
            sum = 0;
        }
    }

    samples.resize(N / decimation);
}

/*===========================================================================*\
 * public function definitions
\*===========================================================================*/
int main(int argc, char *argv[])
{
    int status;
    uint32_t frequency = 0;
    FILE* fp;
    int dev_index;

    install_signal_handler();

    static const struct option long_options[] = {
        {"frequency", required_argument, 0, 'f'},
        {0, 0, 0, 0}
    };

    for (;;) {
        int c = getopt_long(argc, argv, "f:", long_options, 0);
        if (c == -1)
            break;

        switch (c) {
            case 'f':
                if (ymn::strtointeger(optarg, frequency) != ymn::strtointeger_conversion_status_e::success) {
                    fprintf(stderr, "Cannot convert '%s' to integer\n", optarg);
                    exit(EXIT_FAILURE);
                }
                break;

            default:
                /* do nothing */
                break;
        }
    }

    if (argc > optind) {
        fp = fopen(argv[optind], "wb");
        if (fp == NULL) {
            fprintf(stderr, "Cannot create '%s'\n", argv[optind]);
            exit(EXIT_FAILURE);
        }
    }
    else
        fp = stdout;

    if (frequency == 0) {
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    dev_index = verbose_device_search("0");
    if (dev_index < 0)
        exit(EXIT_FAILURE);

    fprintf(stderr, "Opening device #%d\n", dev_index);
    status = rtlsdr_open(&rtlsdr_device, (uint32_t)dev_index);
    if (status < 0) {
        fprintf(stderr, "Failed to open rtlsdr device #%d\n", dev_index);
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, " - done\n");

    fprintf(stderr, "Setting tuner gain to automatic\n");
    status = rtlsdr_set_tuner_gain_mode(rtlsdr_device, 0);
    if (status) {
        fprintf(stderr, "rtlsdr_set_tuner_gain_mode(0) failed\n");
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, " - done\n");

    /* Reset endpoint before we start reading from it (mandatory) */
    fprintf(stderr, "Resseting rtlsdr buffers\n");
    status = rtlsdr_reset_buffer(rtlsdr_device);
    if (status) {
        fprintf(stderr, "rtlsdr_reset_buffer() failed\n");
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, " - done\n");

    frequency += RTL_SDR_SAMPLE_RATE / 4;

    fprintf(stderr, "Setting center frequency to %u Hz\n", frequency);
    status = rtlsdr_set_center_freq(rtlsdr_device, frequency);
    if (status) {
        fprintf(stderr, "rtlsdr_set_center_freq(%u) failed\n", frequency);
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, " - done\n");

    fprintf(stderr, "Setting sample rate to %u Hz\n", RTL_SDR_SAMPLE_RATE);
    status = rtlsdr_set_sample_rate(rtlsdr_device, RTL_SDR_SAMPLE_RATE);
    if (status) {
        fprintf(stderr, "rtlsdr_set_sample_rate(%u) failed\n", RTL_SDR_SAMPLE_RATE);
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, " - done\n");

    fprintf(stderr, "Intermediate sampling rate: %d Hz\n", IF_SAMPLE_RATE);
    fprintf(stderr, "Audio sampling rate: %d Hz\n", AUDIO_SAMPLE_RATE);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto producer = [&](ymn::iringbuffer<ymn::pipeline::buffer_uptr>* irb, ymn::oringbuffer<ymn::pipeline::buffer_uptr>* orb){

        assert(irb == nullptr);
        assert(orb != nullptr);

        int status;
        int n_read;
        static std::size_t counter = 0;

        status = rtlsdr_read_sync(rtlsdr_device, iqbuf_u8, sizeof(iqbuf_u8), &n_read);
        if (status) {
            fprintf(stderr, "rtlsdr_read_sync(%zu) failed\n", sizeof(iqbuf_u8));
            return false;
        }

        if (n_read != sizeof(iqbuf_u8)) {
            fprintf(stderr, "rtlsdr_read_sync(%zu) dropped samples - received %d\n",
                sizeof(iqbuf_u8), n_read);
            return true;
        }

        if (counter++ < IDLE_LOOPS_NUM)
            return true;

        polar_rotate_90(iqbuf_u8);

        iq_buffer_uptr iqbuf_uptr = std::make_unique<buffer<iq_t>>(n_read / 2);
        iq_t* iqbuf = iqbuf_uptr->vector.data();

        /* scale [0, 255] -> [-127, 128] */
        /* scale [-127, 128] -> [-32512, 32768] */
        for (std::size_t i = 0; i < iqbuf_uptr->vector.size(); ++i) {
            iqbuf[i].real((iqbuf_u8[2 * i + 0] - 127) * 256);
            iqbuf[i].imag((iqbuf_u8[2 * i + 1] - 127) * 256);
        }

        long write_status = orb->write(std::move(iqbuf_uptr));
        if (write_status != 1) {
            fprintf(stderr, "%s: orb->write() failed\n", __PRETTY_FUNCTION__);
            fprintf(stderr, "%s\n", orb->to_string().c_str());
        }

        return true;
    };

    auto fm_stage = [&](ymn::iringbuffer<ymn::pipeline::buffer_uptr>* irb, ymn::oringbuffer<ymn::pipeline::buffer_uptr>* orb){

        assert(irb != nullptr);
        assert(orb != nullptr);

        iq_buffer_uptr iqbuf_uptr = get_iq_buffer_uptr(irb);
        if (!iqbuf_uptr)
            return false;

        low_pass_filter(iqbuf_uptr->vector, OVERSAMPLING_2);

        pcm_buffer_uptr pcmbuf_uptr = std::make_unique<buffer<pcm_t>>(iqbuf_uptr->vector.size());

        fm_demod(pcmbuf_uptr->vector.data(), iqbuf_uptr->vector.data(), iqbuf_uptr->vector.size());

        low_pass_filter(pcmbuf_uptr->vector, OVERSAMPLING_1);

        long write_status = orb->write(std::move(pcmbuf_uptr));
        if (write_status != 1) {
            fprintf(stderr, "%s: orb->write() failed\n", __PRETTY_FUNCTION__);
            fprintf(stderr, "%s\n", orb->to_string().c_str());
        }

        return true;
    };

    auto consumer = [&](ymn::iringbuffer<ymn::pipeline::buffer_uptr>* irb, ymn::oringbuffer<ymn::pipeline::buffer_uptr>* orb){

        assert(irb != nullptr);
        assert(orb == nullptr);

        pcm_buffer_uptr pcmbuf_uptr = get_pcm_buffer_uptr(irb);
        if (!pcmbuf_uptr)
            return false;

        fwrite(pcmbuf_uptr->vector.data(), sizeof(pcm_t), pcmbuf_uptr->vector.size(), fp);

        return true;
    };

    ymn::pipeline::stage_function functions[] = {producer, fm_stage, consumer};
    pipeline = std::make_unique<ymn::pipeline>(functions, 42);

    pipeline->start();
    pipeline->join();

    rtlsdr_close(rtlsdr_device);

    if (fp != stdout)
        fclose(fp);

    return 0;
}

/*===========================================================================*\
 * local function definitions
\*===========================================================================*/
static void print_usage(const char* progname)
{
    fprintf(stderr, "usage: %s -f <frequency> [<filename>]\n", progname);
    fprintf(stderr, " options:\n");
    fprintf(stderr, "  -f <frequency>  --frequency=<frequency> : center frequency to tune to\n");
    fprintf(stderr, "  <filename>                              : print output values to this file (default: stdout)\n");
}

static void signal_handler(int signum)
{
    fprintf(stderr, "caught signal %d, terminating ...\n", signum);
    if (pipeline != nullptr)
        pipeline->stop();
    fprintf(stderr, "done\n");
}

static void install_signal_handler(void)
{
    struct sigaction sigact;

    sigact.sa_handler = signal_handler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;

    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);
    sigaction(SIGQUIT, &sigact, NULL);
    sigaction(SIGPIPE, &sigact, NULL);
}

static void fm_demod(pcm_t *pcmbuf, iq_t* iqbuf, const std::size_t N)
{
    pcm_t pcm;
    static iq_t previous{0, 0};

    if (N > 0) {
        pcm = polar_discriminator(iqbuf[0], previous);
        pcmbuf[0] = pcm;

        for (std::size_t n = 1; n < N; ++n) {
            pcm = polar_discriminator(iqbuf[n], iqbuf[n-1]);
            pcmbuf[n] = pcm;
        }

        previous = iqbuf[N - 1];
    }
}

static int verbose_device_search(const char *s)
{
    int i, device_count, device, offset;
    char *s2;
    char vendor[256], product[256], serial[256];

    device_count = rtlsdr_get_device_count();
    if (!device_count) {
        fprintf(stderr, "No supported devices found\n");
        return -1;
    }

    fprintf(stderr, "Found %d device(s):\n", device_count);
    for (i = 0; i < device_count; i++) {
        rtlsdr_get_device_usb_strings(i, vendor, product, serial);
        fprintf(stderr, "  %d:  %s, %s, SN: %s\n", i, vendor, product, serial);
    }
    fprintf(stderr, "\n");

    /* does string look like raw id number */
    device = (int)strtol(s, &s2, 0);
    if (s2[0] == '\0' && device >= 0 && device < device_count) {
        fprintf(stderr, "Using device %d: %s\n",
            device, rtlsdr_get_device_name((uint32_t)device));
        return device;
    }

    /* does string exact match a serial */
    for (i = 0; i < device_count; i++) {
        rtlsdr_get_device_usb_strings(i, vendor, product, serial);
        if (strcmp(s, serial) != 0) {
            continue;}
        device = i;
        fprintf(stderr, "Using device %d: %s\n",
            device, rtlsdr_get_device_name((uint32_t)device));
        return device;
    }

    /* does string prefix match a serial */
    for (i = 0; i < device_count; i++) {
        rtlsdr_get_device_usb_strings(i, vendor, product, serial);
        if (strncmp(s, serial, strlen(s)) != 0) {
            continue;}
        device = i;
        fprintf(stderr, "Using device %d: %s\n",
            device, rtlsdr_get_device_name((uint32_t)device));
        return device;
    }

    /* does string suffix match a serial */
    for (i = 0; i < device_count; i++) {
        rtlsdr_get_device_usb_strings(i, vendor, product, serial);
        offset = strlen(serial) - strlen(s);
        if (offset < 0) {
            continue;}
        if (strncmp(s, serial+offset, strlen(s)) != 0) {
            continue;}
        device = i;
        fprintf(stderr, "Using device %d: %s\n",
            device, rtlsdr_get_device_name((uint32_t)device));
        return device;
    }

    fprintf(stderr, "No matching devices found\n");

    return -1;
}

