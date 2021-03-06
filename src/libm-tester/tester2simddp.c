//          Copyright Naoki Shibata 2010 - 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <mpfr.h>
#include <time.h>
#include <float.h>
#include <limits.h>
#include <math.h>

#ifdef ENABLE_SYS_getrandom
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#endif

#include "sleef.h"

#define DORENAME

#ifdef ENABLE_SSE2
#define CONFIG 2
#include "helpersse2.h"
#include "renamesse2.h"
typedef Sleef___m128d_2 vdouble2;
typedef Sleef___m128_2 vfloat2;
#endif

#ifdef ENABLE_AVX
#define CONFIG 1
#include "helperavx.h"
#include "renameavx.h"
typedef Sleef___m256d_2 vdouble2;
typedef Sleef___m256_2 vfloat2;
#endif

#ifdef ENABLE_FMA4
#define CONFIG 4
#include "helperavx.h"
#include "renamefma4.h"
typedef Sleef___m256d_2 vdouble2;
typedef Sleef___m256_2 vfloat2;
#endif

#ifdef ENABLE_AVX2
#define CONFIG 1
#include "helperavx2.h"
#include "renameavx2.h"
typedef Sleef___m256d_2 vdouble2;
typedef Sleef___m256_2 vfloat2;
#endif

#ifdef ENABLE_AVX512F
#define CONFIG 1
#include "helperavx512f.h"
#include "renameavx512f.h"
typedef Sleef___m512d_2 vdouble2;
typedef Sleef___m512_2 vfloat2;
#endif

#ifdef ENABLE_VECEXT
#define CONFIG 1
#include "helpervecext.h"
#include "norename.h"
#endif

#ifdef ENABLE_PUREC
#define CONFIG 1
#include "helperpurec.h"
#include "norename.h"
#endif

mpfr_t fra, frb, frc, frd, frw, frx, fry, frz;

#define DENORMAL_DBL_MIN (4.94066e-324)

double countULP(double d, mpfr_t c) {
  double c2 = mpfr_get_d(c, GMP_RNDN);
  if (c2 == 0 && d != 0) return 10000;
  if (!isfinite(c2) && !isfinite(d)) return 0;

  int e;
  frexpl(mpfr_get_d(c, GMP_RNDN), &e);
  mpfr_set_ld(frw, fmaxl(ldexpl(1.0, e-53), DENORMAL_DBL_MIN), GMP_RNDN);

  mpfr_set_d(frd, d, GMP_RNDN);
  mpfr_sub(fry, frd, c, GMP_RNDN);
  mpfr_div(fry, fry, frw, GMP_RNDN);
  double u = fabs(mpfr_get_d(fry, GMP_RNDN));

  return u;
}

double countULP2(double d, mpfr_t c) {
  double c2 = mpfr_get_d(c, GMP_RNDN);
  if (c2 == 0 && d != 0) return 10000;
  if (!isfinite(c2) && !isfinite(d)) return 0;

  int e;
  frexpl(mpfr_get_d(c, GMP_RNDN), &e);
  mpfr_set_ld(frw, fmaxl(ldexpl(1.0, e-53), DBL_MIN), GMP_RNDN);

  mpfr_set_d(frd, d, GMP_RNDN);
  mpfr_sub(fry, frd, c, GMP_RNDN);
  mpfr_div(fry, fry, frw, GMP_RNDN);
  double u = fabs(mpfr_get_d(fry, GMP_RNDN));

  return u;
}

typedef union {
  double d;
  uint64_t u64;
  int64_t i64;
} conv_t;

double rnd_fr() {
  conv_t c;
  do {
#ifdef ENABLE_SYS_getrandom
    syscall(SYS_getrandom, &c.u64, sizeof(c.u64), 0);
#else
    c.u64 = random() | ((uint64_t)random() << 31) | ((uint64_t)random() << 62);
#endif
  } while(!isfinite(c.d));
  return c.d;
}

double rnd_zo() {
  conv_t c;
  do {
#ifdef ENABLE_SYS_getrandom
    syscall(SYS_getrandom, &c.u64, sizeof(c.u64), 0);
#else
    c.u64 = random() | ((uint64_t)random() << 31) | ((uint64_t)random() << 62);
#endif
  } while(!isfinite(c.d) || c.d < -1 || 1 < c.d);
  return c.d;
}

void sinpifr(mpfr_t ret, double d) {
  mpfr_t frpi, frd;
  mpfr_inits(frpi, frd, NULL);

  mpfr_const_pi(frpi, GMP_RNDN);
  mpfr_set_d(frd, 1.0, GMP_RNDN);
  mpfr_mul(frpi, frpi, frd, GMP_RNDN);
  mpfr_set_d(frd, d, GMP_RNDN);
  mpfr_mul(frd, frpi, frd, GMP_RNDN);
  mpfr_sin(ret, frd, GMP_RNDN);

  mpfr_clears(frpi, frd, NULL);
}

void cospifr(mpfr_t ret, double d) {
  mpfr_t frpi, frd;
  mpfr_inits(frpi, frd, NULL);

  mpfr_const_pi(frpi, GMP_RNDN);
  mpfr_set_d(frd, 1.0, GMP_RNDN);
  mpfr_mul(frpi, frpi, frd, GMP_RNDN);
  mpfr_set_d(frd, d, GMP_RNDN);
  mpfr_mul(frd, frpi, frd, GMP_RNDN);
  mpfr_cos(ret, frd, GMP_RNDN);

  mpfr_clears(frpi, frd, NULL);
}

vdouble vset(vdouble v, int idx, double d) {
  double a[VECTLENDP];
  vstoreu_v_p_vd(a, v);
  a[idx] = d;
  return vloadu_vd_p(a);
}

double vget(vdouble v, int idx) {
  double a[VECTLENDP];
  vstoreu_v_p_vd(a, v);
  return a[idx];
}

int main(int argc,char **argv)
{
  mpfr_set_default_prec(256);
  mpfr_inits(fra, frb, frc, frd, frw, frx, fry, frz, NULL);

  conv_t cd;
  double d, t;
  vdouble vd = vcast_vd_d(0);
  vdouble vd2 = vcast_vd_d(0);
  vdouble vzo = vcast_vd_d(0);
  vdouble vad = vcast_vd_d(0);
  vdouble2 sc, sc2;
  int cnt;
  
  srandom(time(NULL));

#if 0
  cd.d = M_PI;
  mpfr_set_d(frx, cd.d, GMP_RNDN);
  cd.i64+=3;
  printf("%g\n", countULP(cd.d, frx));
#endif

  const double rangemax = 1e+14; // 2^(24*2-1)
  
  for(cnt = 0;;cnt++) {
    int e = cnt % VECTLENDP;
    switch(cnt & 7) {
    case 0:
      d = (2 * (double)random() / RAND_MAX - 1) * rangemax;
      break;
    case 1:
      cd.d = rint((2 * (double)random() / RAND_MAX - 1) * rangemax) * M_PI_4;
      cd.i64 += (random() & 31) - 15;
      d = cd.d;
      break;
    case 2:
      d = (2 * (double)random() / RAND_MAX - 1) * 1e+7;
      break;
    case 3:
      cd.d = rint((2 * (double)random() / RAND_MAX - 1) * 1e+7) * M_PI_4;
      cd.i64 += (random() & 31) - 15;
      d = cd.d;
      break;
    case 4:
      d = (2 * (double)random() / RAND_MAX - 1) * 10000;
      break;
    case 5:
      cd.d = rint((2 * (double)random() / RAND_MAX - 1) * 10000) * M_PI_4;
      cd.i64 += (random() & 31) - 15;
      d = cd.d;
      break;
    default:
      d = rnd_fr();
      break;
    }

    if (!isfinite(d)) continue;

    vd = vset(vd, e, d);

    sc  = xsincospi_u05(vd);
    sc2 = xsincospi_u35(vd);

    {
      const double rangemax2 = 1e+9/4;
      
      sinpifr(frx, d);

      double u0 = countULP2(t = vget(sc.x, e), frx);

      if ((fabs(d) <= rangemax2 && u0 > 0.505) || fabs(t) > 1 || !isfinite(t)) {
	printf(ISANAME " sincospi_u05 sin arg=%.20g ulp=%.20g\n", d, u0);
      }

      double u1 = countULP2(t = vget(sc2.x, e), frx);

      if ((fabs(d) <= rangemax2 && u1 > 1.5) || fabs(t) > 1 || !isfinite(t)) {
	printf(ISANAME " sincospi_u35 sin arg=%.20g ulp=%.20g\n", d, u1);
      }
    }

    {
      const double rangemax2 = 1e+9/4;
      
      cospifr(frx, d);

      double u0 = countULP2(t = vget(sc.y, e), frx);

      if ((fabs(d) <= rangemax2 && u0 > 0.505) || fabs(t) > 1 || !isfinite(t)) {
	printf(ISANAME " sincospi_u05 cos arg=%.20g ulp=%.20g\n", d, u0);
      }

      double u1 = countULP2(t = vget(sc.y, e), frx);

      if ((fabs(d) <= rangemax2 && u1 > 1.5) || fabs(t) > 1 || !isfinite(t)) {
	printf(ISANAME " sincospi_u35 cos arg=%.20g ulp=%.20g\n", d, u1);
      }
    }
    
    sc = xsincos(vd);
    sc2 = xsincos_u1(vd);
    
    {
      mpfr_set_d(frx, d, GMP_RNDN);
      mpfr_sin(frx, frx, GMP_RNDN);

      double u0 = countULP(t = vget(xsin(vd), e), frx);
      
      if ((fabs(d) <= rangemax && u0 > 3.5) || fabs(t) > 1 || !isfinite(t)) {
	printf(ISANAME " sin arg=%.20g ulp=%.20g\n", d, u0);
	fflush(stdout);
      }

      double u1 = countULP(t = vget(sc.x, e), frx);
      
      if ((fabs(d) <= rangemax && u1 > 3.5) || fabs(t) > 1 || !isfinite(t)) {
	printf(ISANAME " sincos sin arg=%.20g ulp=%.20g\n", d, u1);
	fflush(stdout);
      }

      double u2 = countULP(t = vget(xsin_u1(vd), e), frx);
      
      if ((fabs(d) <= rangemax && u2 > 1) || fabs(t) > 1 || !isfinite(t)) {
	printf(ISANAME " sin_u1 arg=%.20g ulp=%.20g\n", d, u2);
	fflush(stdout);
      }

      double u3 = countULP(t = vget(sc2.x, e), frx);
      
      if ((fabs(d) <= rangemax && u3 > 1) || fabs(t) > 1 || !isfinite(t)) {
	printf(ISANAME " sincos_u1 sin arg=%.20g ulp=%.20g\n", d, u3);
	fflush(stdout);
      }
    }

    {
      mpfr_set_d(frx, d, GMP_RNDN);
      mpfr_cos(frx, frx, GMP_RNDN);

      double u0 = countULP(t = vget(xcos(vd), e), frx);
      
      if ((fabs(d) <= rangemax && u0 > 3.5) || fabs(t) > 1 || !isfinite(t)) {
	printf(ISANAME " cos arg=%.20g ulp=%.20g\n", d, u0);
	fflush(stdout);
      }

      double u1 = countULP(t = vget(sc.y, e), frx);
      
      if ((fabs(d) <= rangemax && u1 > 3.5) || fabs(t) > 1 || !isfinite(t)) {
	printf(ISANAME " sincos cos arg=%.20g ulp=%.20g\n", d, u1);
	fflush(stdout);
      }

      double u2 = countULP(t = vget(xcos_u1(vd), e), frx);
      
      if ((fabs(d) <= rangemax && u2 > 1) || fabs(t) > 1 || !isfinite(t)) {
	printf(ISANAME " cos_u1 arg=%.20g ulp=%.20g\n", d, u2);
	fflush(stdout);
      }

      double u3 = countULP(t = vget(sc2.y, e), frx);
      
      if ((fabs(d) <= rangemax && u3 > 1) || fabs(t) > 1 || !isfinite(t)) {
	printf(ISANAME " sincos_u1 cos arg=%.20g ulp=%.20g\n", d, u3);
	fflush(stdout);
      }
    }

    {
      mpfr_set_d(frx, d, GMP_RNDN);
      mpfr_tan(frx, frx, GMP_RNDN);

      double u0 = countULP(t = vget(xtan(vd), e), frx);
      
      if ((fabs(d) < 1e+7 && u0 > 3.5) || (fabs(d) <= rangemax && u0 > 5) || isnan(t)) {
	printf(ISANAME " tan arg=%.20g ulp=%.20g\n", d, u0);
	fflush(stdout);
      }

      double u1 = countULP(t = vget(xtan_u1(vd), e), frx);
      
      if ((fabs(d) <= rangemax && u1 > 1) || isnan(t)) {
	printf(ISANAME " tan_u1 arg=%.20g ulp=%.20g\n", d, u1);
	fflush(stdout);
      }
    }

    d = rnd_fr();
    double d2 = rnd_fr(), zo = rnd_zo();
    vd = vset(vd, e, d);
    vd2 = vset(vd2, e, d2);
    vzo = vset(vzo, e, zo);
    vad = vset(vad, e, fabs(d));

    
    {
      mpfr_set_d(frx, fabs(d), GMP_RNDN);
      mpfr_log(frx, frx, GMP_RNDN);

      double u0 = countULP(t = vget(xlog(vad), e), frx);
      
      if (u0 > 3.5) {
	printf(ISANAME " log arg=%.20g ulp=%.20g\n", d, u0);
	fflush(stdout);
      }

      double u1 = countULP(t = vget(xlog_u1(vad), e), frx);
      
      if (u1 > 1) {
	printf(ISANAME " log_u1 arg=%.20g ulp=%.20g\n", d, u1);
	fflush(stdout);
      }
    }
    
    {
      mpfr_set_d(frx, fabs(d), GMP_RNDN);
      mpfr_log10(frx, frx, GMP_RNDN);

      double u0 = countULP(t = vget(xlog10(vad), e), frx);
      
      if (u0 > 1) {
	printf(ISANAME " log10 arg=%.20g ulp=%.20g\n", d, u0);
	fflush(stdout);
      }
    }

    {
      mpfr_set_d(frx, d, GMP_RNDN);
      mpfr_log1p(frx, frx, GMP_RNDN);

      double u0 = countULP(t = vget(xlog1p(vd), e), frx);
      
      if ((-1 <= d && d <= 1e+307 && u0 > 1) ||
	  (d < -1 && !isnan(t)) ||
	  (d > 1e+307 && !(u0 <= 1 || isinf(t)))) {
	printf(ISANAME " log1p arg=%.20g ulp=%.20g\n", d, u0);
	fflush(stdout);
      }
    }

    {
      mpfr_set_d(frx, d, GMP_RNDN);
      mpfr_exp(frx, frx, GMP_RNDN);

      double u0 = countULP(t = vget(xexp(vd), e), frx);
      
      if (u0 > 1) {
	printf(ISANAME " exp arg=%.20g ulp=%.20g\n", d, u0);
	fflush(stdout);
      }
    }
    
    {
      mpfr_set_d(frx, d, GMP_RNDN);
      mpfr_exp2(frx, frx, GMP_RNDN);

      double u0 = countULP(t = vget(xexp2(vd), e), frx);
      
      if (u0 > 1) {
	printf(ISANAME " exp2 arg=%.20g ulp=%.20g\n", d, u0);
	fflush(stdout);
      }
    }
    
    {
      mpfr_set_d(frx, d, GMP_RNDN);
      mpfr_exp10(frx, frx, GMP_RNDN);

      double u0 = countULP(t = vget(xexp10(vd), e), frx);
      
      if (u0 > 1) {
	printf(ISANAME " exp10 arg=%.20g ulp=%.20g\n", d, u0);
	fflush(stdout);
      }
    }
    
    {
      mpfr_set_d(frx, d, GMP_RNDN);
      mpfr_expm1(frx, frx, GMP_RNDN);

      double u0 = countULP(t = vget(xexpm1(vd), e), frx);
      
      if (u0 > 1) {
	printf(ISANAME " expm1 arg=%.20g ulp=%.20g\n", d, u0);
	fflush(stdout);
      }
    }
    
    {
      mpfr_set_d(frx, d, GMP_RNDN);
      mpfr_set_d(fry, d2, GMP_RNDN);
      mpfr_pow(frx, fry, frx, GMP_RNDN);

      double u0 = countULP(t = vget(xpow(vd2, vd), e), frx);
      
      if (u0 > 1) {
	printf(ISANAME " pow arg=%.20g, %.20g ulp=%.20g\n", d2, d, u0);
	fflush(stdout);
      }
    }
    
    {
      mpfr_set_d(frx, d, GMP_RNDN);
      mpfr_cbrt(frx, frx, GMP_RNDN);

      double u0 = countULP(t = vget(xcbrt(vd), e), frx);
      
      if (u0 > 3.5) {
	printf(ISANAME " cbrt arg=%.20g ulp=%.20g\n", d, u0);
	fflush(stdout);
      }

      double u1 = countULP(t = vget(xcbrt_u1(vd), e), frx);
      
      if (u1 > 1) {
	printf(ISANAME " cbrt_u1 arg=%.20g ulp=%.20g\n", d, u1);
	fflush(stdout);
      }
    }
    
    {
      mpfr_set_d(frx, zo, GMP_RNDN);
      mpfr_asin(frx, frx, GMP_RNDN);

      double u0 = countULP(t = vget(xasin(vzo), e), frx);
      
      if (u0 > 3.5) {
	printf(ISANAME " asin arg=%.20g ulp=%.20g\n", d, u0);
	fflush(stdout);
      }

      double u1 = countULP(t = vget(xasin_u1(vzo), e), frx);
      
      if (u1 > 1) {
	printf(ISANAME " asin_u1 arg=%.20g ulp=%.20g\n", d, u1);
	fflush(stdout);
      }
    }

    {
      mpfr_set_d(frx, zo, GMP_RNDN);
      mpfr_acos(frx, frx, GMP_RNDN);

      double u0 = countULP(t = vget(xacos(vzo), e), frx);
      
      if (u0 > 3.5) {
	printf(ISANAME " acos arg=%.20g ulp=%.20g\n", d, u0);
	fflush(stdout);
      }

      double u1 = countULP(t = vget(xacos_u1(vzo), e), frx);
      
      if (u1 > 1) {
	printf(ISANAME " acos_u1 arg=%.20g ulp=%.20g\n", d, u1);
	fflush(stdout);
      }
    }
    
    {
      mpfr_set_d(frx, d, GMP_RNDN);
      mpfr_atan(frx, frx, GMP_RNDN);

      double u0 = countULP(t = vget(xatan(vd), e), frx);
      
      if (u0 > 3.5) {
	printf(ISANAME " atan arg=%.20g ulp=%.20g\n", d, u0);
	fflush(stdout);
      }

      double u1 = countULP(t = vget(xatan_u1(vd), e), frx);
      
      if (u1 > 1) {
	printf(ISANAME " atan_u1 arg=%.20g ulp=%.20g\n", d, u1);
	fflush(stdout);
      }
    }
    
    {
      mpfr_set_d(frx, d, GMP_RNDN);
      mpfr_set_d(fry, d2, GMP_RNDN);
      mpfr_atan2(frx, fry, frx, GMP_RNDN);

      double u0 = countULP(t = vget(xatan2(vd2, vd), e), frx);
      
      if (u0 > 3.5) {
	printf(ISANAME " atan2 arg=%.20g, %.20g ulp=%.20g\n", d2, d, u0);
	fflush(stdout);
      }

      double u1 = countULP2(t = vget(xatan2_u1(vd2, vd), e), frx);
      
      if (u1 > 1) {
	printf(ISANAME " atan2_u1 arg=%.20g, %.20g ulp=%.20g\n", d2, d, u1);
	fflush(stdout);
      }
    }
    
    {
      mpfr_set_d(frx, d, GMP_RNDN);
      mpfr_sinh(frx, frx, GMP_RNDN);

      double u0 = countULP(t = vget(xsinh(vd), e), frx);
      
      if ((fabs(d) <= 709 && u0 > 1) ||
	  (d >  709 && !(u0 <= 1 || (isinf(t) && t > 0))) ||
	  (d < -709 && !(u0 <= 1 || (isinf(t) && t < 0)))) {
	printf(ISANAME " sinh arg=%.20g ulp=%.20g\n", d, u0);
	fflush(stdout);
      }
    }
    
    {
      mpfr_set_d(frx, d, GMP_RNDN);
      mpfr_cosh(frx, frx, GMP_RNDN);

      double u0 = countULP(t = vget(xcosh(vd), e), frx);
      
      if ((fabs(d) <= 709 && u0 > 1) || !(u0 <= 1 || (isinf(t) && t > 0))) {
	printf(ISANAME " cosh arg=%.20g ulp=%.20g\n", d, u0);
	fflush(stdout);
      }
    }
    
    {
      mpfr_set_d(frx, d, GMP_RNDN);
      mpfr_tanh(frx, frx, GMP_RNDN);

      double u0 = countULP(t = vget(xtanh(vd), e), frx);
      
      if (u0 > 1) {
	printf(ISANAME " tanh arg=%.20g ulp=%.20g\n", d, u0);
	fflush(stdout);
      }
    }
    
    {
      mpfr_set_d(frx, d, GMP_RNDN);
      mpfr_asinh(frx, frx, GMP_RNDN);

      double u0 = countULP(t = vget(xasinh(vd), e), frx);
      
      if ((fabs(d) < sqrt(DBL_MAX) && u0 > 1) ||
	  (d >=  sqrt(DBL_MAX) && !(u0 <= 1 || (isinf(t) && t > 0))) ||
	  (d <= -sqrt(DBL_MAX) && !(u0 <= 1 || (isinf(t) && t < 0)))) {
	printf(ISANAME " asinh arg=%.20g ulp=%.20g\n", d, u0);
	fflush(stdout);
      }
    }
    
    {
      mpfr_set_d(frx, d, GMP_RNDN);
      mpfr_acosh(frx, frx, GMP_RNDN);

      double u0 = countULP(t = vget(xacosh(vd), e), frx);
      
      if ((fabs(d) < sqrt(DBL_MAX) && u0 > 1) ||
	  (d >=  sqrt(DBL_MAX) && !(u0 <= 1 || (isinf(t) && t > 0))) ||
	  (d <= -sqrt(DBL_MAX) && !isnan(t))) {
	printf(ISANAME " acosh arg=%.20g ulp=%.20g\n", d, u0);
	fflush(stdout);
      }
    }
    
    {
      mpfr_set_d(frx, d, GMP_RNDN);
      mpfr_atanh(frx, frx, GMP_RNDN);

      double u0 = countULP(t = vget(xatanh(vd), e), frx);
      
      if (u0 > 1) {
	printf(ISANAME " atanh arg=%.20g ulp=%.20g\n", d, u0);
	fflush(stdout);
      }
    }
  }
}
