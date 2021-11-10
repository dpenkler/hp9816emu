#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <math.h>

#define SAMP_FREQ        16000
#define MAX_DURATION     256       // centiseconds
#define MAX_SND_BUF_LEN  MAX_DURATION*SAMP_FREQ/100
#define NUM_SND_CHANS    1
#define AMPLITUDE        32767
static int dspfd=-1;              /* SOUND file descriptor              */
static int dsplen=2048;           /* buffer size in samples             */
static int dspfs=0;               /* current sampling frequency         */
static char *snd_file="/dev/dsp";

void sound_close();

static int sound_setfs(int fs) {
  int tmp = fs;
  if (ioctl(dspfd, SNDCTL_DSP_SPEED, &tmp)) {
    fprintf(stderr,"ioctl SNDCTL_DSP_SPEED failed\n");
    sound_close();
    return 0;
  } else if (fs != tmp) {
    fprintf(stderr,"SampFreq %d unsupported, gives %d\n",fs,tmp);
    sound_close();
    return 0;
  } else dspfs = fs;
  return 1;
}

static int sound_set_chan(int n) {
  int tmp = n;
  if (ioctl(dspfd, SNDCTL_DSP_CHANNELS, &tmp)) {
    fprintf(stderr,"ioctl SNDCTL_DSP_CHANNELS failed\n");
    sound_close();
    return 0;
  }
  if (tmp != n) {
    fprintf(stderr,"Couldn't set number of channels on /dev/dsp\n");
    sound_close();
    return 0;
  }
  return 1;
}

static int sound_sync() {
  if (ioctl(dspfd, SNDCTL_DSP_POST, NULL) < 0) {
    fprintf(stderr,"ioctl dsp post failed\n");
    return 0;
  } else {
    return 1;
  }
}

static int sound_get_len() {
  int tmp;
  ioctl (dspfd, SNDCTL_DSP_GETBLKSIZE, &tmp);
  if (tmp == -1) {
    fprintf(stderr,"ioctl SNDCTL_DSP_GETBLKSIZE failed");
    return -1;
  }
 return tmp/2; /* we work in short samples */
}

int sound_init() {
  /* return working sample buffer len */
  int tmp;
  static int samplesize = 16;

  if ((dspfd = open (snd_file, O_RDWR, 0)) == -1) {
    fprintf(stderr,"Cannot open snd_file\n");
    return 0;
  }

  // ioctl(dspfd, SNDCTL_DSP_RESET, 0);

#if 1
  tmp = 0x100007;
  if (ioctl(dspfd, SNDCTL_DSP_SETFRAGMENT, &tmp)) {
    fprintf (stderr," unable to set fragment size on snd_file\n");
    sound_close();
    return 0;
  }
#endif
  
  tmp = samplesize;
  if (ioctl(dspfd, SNDCTL_DSP_SAMPLESIZE, &samplesize) == -1) {
    fprintf(stderr," unable to set sample size on snd_file\n");
    sound_close();
    return 0;
  }
  if (tmp != samplesize) {
    fprintf(stderr,"Sample size %d not supported!\n",samplesize);
    sound_close();
    return 0;
  }

  if (!sound_set_chan(NUM_SND_CHANS)) return 0;

  if (!sound_setfs(SAMP_FREQ)) return 0;

  dsplen = sound_get_len();

  fprintf(stderr,"dsplen: %d\n",dsplen);
  
  return dsplen > 0 ? 1 : 0;
}

static short dspout[MAX_SND_BUF_LEN];

void sound_close() {
  if (ioctl(dspfd, SNDCTL_DSP_RESET, 0)) 
    fprintf(stderr,"Couldn't reset DSP on close\n");
  close(dspfd);
  dspfd = -1;
}

void emuBeep(int f, int d) { // frequency duration
  int k,nsamps,blen,len;
  double freq,rps;
  double pi = 3.141592653589793238462643383279; 
  if (dspfd < 0) fprintf(stderr,"emuBeep sound not initialised");
  fprintf(stderr,"emuBeep: %d %d\n",f,d);
  if (!f) return; // * zero freq => no sound
  nsamps = d*dspfs/100;
  freq = f*81.38; // Pascal 3.2 Procedure Library 14-9
  rps = (freq*2*pi)/dspfs;
  for (k=0; k<nsamps; k++) dspout[k] = 32767*sin(rps*k);
  blen = nsamps*sizeof(short);
  len = write(dspfd,dspout,blen);
  if (len < 0) {
    fprintf(stderr,"write to dsp failed\n");
    return;
  }
  if (len != blen)
    fprintf(stderr,"Short write to dsp wanted %d got %d\n",blen,len);
  sound_sync();
}
