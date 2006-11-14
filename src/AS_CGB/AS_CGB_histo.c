
/**************************************************************************
 * This file is part of Celera Assembler, a software program that 
 * assembles whole-genome shotgun reads into contigs and scaffolds.
 * Copyright (C) 1999-2004, Applera Corporation. All rights reserved.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received (LICENSE.txt) a copy of the GNU General Public 
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *************************************************************************/
/*********************************************************************
 * $Id: AS_CGB_histo.c,v 1.8 2006-11-14 19:58:21 eliv Exp $
 * Module:  AS_CGB_histo.c
 * Description: A histogramming routine and auxillary functions.
 * Assumptions:
 *********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "AS_global.h"
#include "AS_CGB_histo.h"

#undef DEBUGGING1
#undef DEBUGGING2

/*************************************************************************/
static char CM_ID[] = "$Id: AS_CGB_histo.c,v 1.8 2006-11-14 19:58:21 eliv Exp $";
/*************************************************************************/

/* Histogram library */

typedef struct { 
  int        cnt;     /* # of data points */
  int        nsample; /* size of sample array */
  /* Number of samples collected before bucketing,
     and number of buckets thereafter */
  int   *thescore;       /* score array */
  int    filled;  /* Have NSAMPLE samples been collected? 
		     Are we in bucket mode? */
#ifdef USE_SYNC
  int   sync;    /* value to sync lower bucket bounds to */
#endif
  int   low;     /* Lowest bucket score, an integer */
  int   hgh;     /* Highest bucket score */
  int   min;
  int   max; 
  int   bucket_width;    /* number of integral score values per bucket */
  int        nbucket;  /* size of bucket array */
  int        *bucket_cnt;  /* bucket array */
  int        *bucket_min;
  int        *bucket_max;
  int        logarithmic;
  /* */
  int        extended;
  HistoDataType   *temp_data;
  HistoDataType   *scan_data;
  HistoDataType   *aggr_data;
  HistoDataType   *sample_data; /* sample_data array */
  HistoDataType   *bucket_data;
  HistoDataType   *(*indexdata)(HistoDataType *b,int ib);
  void       (*setdata)(HistoDataType *a,int ib,HistoDataType *b);
  void       (*aggregate)(HistoDataType *a,int ib,HistoDataType *b);
  void       (*printdata)(FILE *fout,HistoDataType *,HistoDataType *,HistoDataType *);
} HISTOGRAM;

Histogram_t *create_histogram(
			    int nsample,
			    int nbucket,
#ifdef USE_SYNC
			    int sync,
#endif
			    int filled,
			    int logarithmic)
/* Create an initially empty histogram where buckets will be
   selected so that "sync" will be the low end of some bucket. */
{ 
  HISTOGRAM *h;
  int *bucket_cnt,*bucket_min,*bucket_max;
  int *thescore;
  h = (HISTOGRAM *) safe_malloc(sizeof(HISTOGRAM));
  thescore   = (int *)safe_calloc(nsample,sizeof(int));
  bucket_cnt = (int *)safe_calloc(nbucket,sizeof(int));
  h->cnt         = 0;
  h->filled      = filled;
#ifdef USE_SYNC
  h->sync        = sync;
#endif
  h->nsample     = nsample;
  h->nbucket     = nbucket;
  h->thescore    = thescore;
  h->bucket_width = 1;
  h->low          = 0;
  h->hgh          = nbucket-1;
  h->min         =  1 << 30;
  h->max         = - h->min;

  h->bucket_cnt  = bucket_cnt;
  h->logarithmic = logarithmic;
    
  /* */
  h->extended    = 0;
  h->temp_data   = NULL;
  h->scan_data   = NULL;
  h->aggr_data   = NULL;
  h->sample_data = NULL;
  h->bucket_data = NULL;
  h->indexdata   = NULL;
  h->setdata     = NULL;
  h->aggregate   = NULL;
  h->printdata   = NULL;
  /* */
  bucket_min = (int *)safe_calloc(nbucket,sizeof(int));
  bucket_max = (int *)safe_calloc(nbucket,sizeof(int));
  h->bucket_min  = bucket_min;
  h->bucket_max  = bucket_max;
  
  if(logarithmic) {
    h->filled = 1;
  }
#ifdef DEBUGGING1
  printf("h->cnt = %d\n",h->cnt);
  printf("h->filled = %d\n",h->filled);
#endif
  return (Histogram_t *)h;
}

void extend_histogram(  
		      Histogram_t * histogram,
		      size_t nbytes,
		      HistoDataType * (*indexdata)(HistoDataType *b,int ib),
		      void (*setdata)(HistoDataType *a,int ib,HistoDataType *b),
		      void (*aggregate)(HistoDataType *a,int ib,HistoDataType *b),
		      void (*printdata)(FILE *fout,
					HistoDataType *data,
					HistoDataType *scan_data,
					HistoDataType *aggr_data)
	)
{
  HISTOGRAM *h = (HISTOGRAM *)histogram;
  HistoDataType *sample_data;
  HistoDataType *bucket_data;
  HistoDataType *temp_data, *scan_data, *aggr_data;

  assert(h != NULL);
  assert(nbytes > 0 );
  sample_data = (HistoDataType *) safe_malloc((h->nsample)*nbytes);
  bucket_data = (HistoDataType *) safe_malloc((h->nbucket)*nbytes);
  temp_data   = (HistoDataType *) safe_malloc(nbytes);
  scan_data   = (HistoDataType *) safe_malloc(nbytes);
  aggr_data   = (HistoDataType *) safe_malloc(nbytes);

  h->temp_data   = temp_data;
  h->scan_data   = scan_data;
  h->aggr_data   = aggr_data;
  h->sample_data = sample_data;
  h->bucket_data = bucket_data;
  h->indexdata   = indexdata;
  h->setdata     = setdata;
  h->aggregate   = aggregate;
  h->printdata   = printdata;
  h->extended    = 1;
}

void free_histogram(Histogram_t *histogram)
/* Free the data structure for histogram "h" */
{ 
  HISTOGRAM *h = (HISTOGRAM *)histogram;
  assert(h != NULL);
  free(h->thescore);
  if(NULL != h->temp_data) { free(h->temp_data);}
  if(NULL != h->scan_data) { free(h->scan_data);}
  if(NULL != h->aggr_data) { free(h->aggr_data);}
  free(h->bucket_cnt);
  free(h->bucket_min);
  free(h->bucket_max);
  if(NULL != h->sample_data) { free(h->sample_data);}
  if(NULL != h->bucket_data) { free(h->bucket_data);}
  free((char *) h); 
}

static int bucket_from_score(HISTOGRAM *h, int thescore) {
  int ib;
  h->max = MAX(thescore,h->max);
  h->min = MIN(thescore,h->min);
  ib = thescore;

  if(! h->logarithmic) {
    ib = (thescore - h->low)/h->bucket_width;
  } else {
    int thesign, decade, divisor, theoffset;
    int middlebucket,ii;
    middlebucket = h->nbucket/2;
    thesign = (thescore < 0 ? -1 : 1 );
    /* decade  = floor(log10(1.*thesign*thescore)); */
    decade=0; for(ii=thesign*thescore; ii>=10; ii /=10 ) { decade++;}
    /* divisor = (int) pow(10.,(double)decade); */
    divisor = 1; for(ii=0;ii<decade;ii++) { divisor *= 10;}
    theoffset = thesign*thescore/divisor;
    ib = thesign*(theoffset + decade*10) + middlebucket;
#if 0
    printf("thescore,thesign,decade,theoffset,ib = %5d %3d %5d %5d %5d\n",
	   thescore,thesign,decade,theoffset,ib);
#endif
  }

  /* Place a floor and ceiling to the histogram. */
  ib = MAX(ib,0);
  ib = MIN(ib,h->nbucket-1);
  return ib;
}

static void fill(HISTOGRAM *h)
{ 
  /* Converts from sample-mode to bucket-mode */
  int low, hgh;

  assert(h != NULL);
  /* At this point h->sample[.] is a data value. */
  if( h->cnt > 0 ) { 
    low = hgh = h->thescore[0];
  } else {
    low = hgh = 0;
  }
  {
    int is;
    for (is = 1; is < h->cnt; is++) {
      const int thescore = h->thescore[is];
      hgh = MAX(hgh,thescore);
      low = MIN(low,thescore);
    }
  }
  /* hgh and low are the bounds on the sampled data values. */
  h->hgh = hgh;
  h->low = low;
  h->max = hgh;
  h->min = low;
  h->bucket_width = (h->hgh - h->low)/(h->nbucket) + 1;
#ifdef DEBUGGING1
  printf("in fill: h->low,h->hgh=%d,%d\n",
	 h->low,h->hgh);
#endif

  {
    int ib;
    for (ib = 0; ib < h->nbucket; ib++) {
      h->bucket_cnt[ib] = 0;
    }
  }
  {
    int is;
    for (is = 0 ; is < h->cnt; is++) {
      const int thescore = h->thescore[is];
      const int ib = bucket_from_score(h,thescore);
#ifdef DEBUGGING1
      printf("in fill is,score,ib = %d,%d,%d\n",is,thescore,ib);
#endif
      h->bucket_cnt[ib] += 1;
      if( h->bucket_cnt[ib] == 1) {
        h->bucket_min[ib] = thescore;
        h->bucket_max[ib] = thescore;
      } else {
        h->bucket_min[ib] = MIN(h->bucket_min[ib],thescore);
        h->bucket_max[ib] = MAX(h->bucket_max[ib],thescore);
      }
      if(h->extended) {
        HistoDataType *data;
#ifdef DEBUGGING1
        printf("heeee %d %d\n",is,ib);
#endif
        data = (h->indexdata)(h->sample_data,is);
        if( h->bucket_cnt[ib] == 1) {
          (h->setdata)(h->bucket_data,ib,data);
        } else {
          (h->aggregate)(h->bucket_data,ib,data);
        }
      }
    }
  }
  h->filled = 1;
}

/* Add data point "data" to histogram "h" */


void add_to_histogram(Histogram_t *histogram, int thescore, HistoDataType *data)
{ 
  HISTOGRAM *h = (HISTOGRAM *)histogram;
  assert(h != NULL);
  assert( !(h->extended) || (data != NULL) );

  /* Compute the histogram total. */
  if(h->extended) {
    if( h->cnt == 0) {
      (h->setdata)(h->aggr_data,0,data);
    } else {
      (h->aggregate)(h->aggr_data,0,data);
    }
  }

  if(( h->cnt == h->nsample) && (!(h->filled)))
    { 
      fprintf(stderr,"CONVERTING HISTOGRAM TO BUCKET MODE\n");
      fill(h);
    }
  if(! (h->filled) ) {
    h->thescore[h->cnt] = thescore;
    if(h->extended) {
      (h->setdata)(h->sample_data,h->cnt,data);
    }
  } else {
    int ib;
    ib = bucket_from_score(h,thescore);
    h->bucket_cnt[ib] += 1;
    if( h->bucket_cnt[ib] == 1) {
      h->bucket_min[ib] = thescore;
      h->bucket_max[ib] = thescore;
    } else {
      h->bucket_min[ib] = MIN(h->bucket_min[ib],thescore);
      h->bucket_max[ib] = MAX(h->bucket_max[ib],thescore);
    }
    if(h->extended) {
      if( h->bucket_cnt[ib] == 1) {
	(h->setdata)(h->bucket_data,ib,data);
      } else {
	(h->aggregate)(h->bucket_data,ib,data);
      }
    }
  }
  h->cnt ++;
}

#if 0
static int compute_cm(HISTOGRAM *h, int rez, int numb) {
  /* 
     Compute the number of buckets per printed bucket.
   */
  int cm; 
  if (rez == 0)
    { cm = 1;}
  else
    { 
      int nbuck_printed,bck;
      nbuck_printed = (numb-1)/rez+1;
      bck = nbuck_printed*(h->bucket_width);
#if 0
      { 
	double nrm;
	nrm = pow(10.,floor(log10(1.*bck)));
	if (bck/nrm < 1.001)
	  bck = nrm;
	else if (bck/nrm < 2.)
	  bck = 2.*nrm;
	else if (bck/nrm < 5.)
	  bck = 5.*nrm;
	else
        bck = 10.*nrm;
	if (bck % h->bucket_width != 0) bck *= 2;
      }
#endif
      cm = bck/h->bucket_width;
    }
  return cm;
}
#endif

#if 0
static int compute_precision(HISTOGRAM *h,int min,int max) 
{
  int i, prec;
  /* "prec" is the number of digits necessary to print the integer. */
  i = (h->low) + (max+1)*(h->bucket_width) - 1;
  if (i == 0)
    { prec = 0;}
  else
    { prec = (int) log10((double)i);}
  i = (h->low) + min*(h->bucket_width);
  if ((i < 0) && (prec < 1. + log10((double)(-i))))
    prec = 1 + (int) log10((double)(-i));
  prec += 1;
  return prec;
}
#endif

#if 0
static int compute_proc(HISTOGRAM *h,int cs,int cm)
{
  int max,ib,proc;
  /* Compute the maximum number of samples in a consolidated bucket.
     Call it "max". */
  max = 0;
  for (ib = cs; ib+cm >= 1; ib -= cm) {
    int j;
    int s;
    s = 0;
    for (j = 0; j < cm; j++)
      if( (ib+j >= 0) && (ib+j < h->nsample))
	{
	  s += h->thescore[ib+j];
	}
    if (s > max) max = s;
  }
  /* max is now the maximum value in a consolidated bucket */
  
  if (max == 0)
    proc = 1;
  else
    proc = (int)log10((double)max) + 1;
  
  return proc;
}
#endif

/* Print a histogram of "h" with approximately "rez" buckets and with
   the  ":" of the output in column "indent".  If "rez" = 0 then histogram
   is printed in the finest bucket resolution possible.  (N.B. A call
   to print histogram has the effect of shifting it to bucket mode
   regardless of how many samples have been added thus far.)  The
   function returns the field width used to print the frequency (in
   case one would like to add some aligned columns after the output). */

void print_histogram(FILE *fout, Histogram_t *histogram, int rez, int indent)
{ 
  HISTOGRAM *h = (HISTOGRAM *)histogram;
  int numb; /* The number of bins that have data. */
  int minb=0; /* The lowest index of a bin with data. */
  int maxb=0; /* The highest index of a bin with data. */
  int cm, cs;
  // The bucket width is cm.
  // The number of buckets is cs.
  int prec = 6; /* the field width used to print the bin size  */
  int proc = 6; /* the field width used to print the frequency */
  int single;
  int first_flag;

#ifdef DEBUGGING1
  printf("h->cnt = %d\n",h->cnt);
  printf("h->filled = %d\n",h->filled);
#endif

  if (h->cnt == 0) 
    { return ;}

#ifdef DEBUGGING2
  {
    int is;
    for (is = 0; is < MIN((h->cnt),(h->nsample)); is++) {
      HistoDataType *data;
      fprintf(fout," %d : %d ",is,h->thescore[is]);
      if(h->extended) {
	data = (h->indexdata)(h->sample_data,is);
	(h->printdata)(fout,data);
      }
      fprintf(fout,"\n");
    }
  }
#endif

  if (! h->filled)
    { fill(h);}

  {
    int ib;
    numb = 0;
    for (ib = 0; ib < h->nbucket; ib++) {
      const int icnt =  h->bucket_cnt[ib];
      if(icnt > 0)
        { if (numb == 0) minb = ib;
        numb += 1;
        maxb = ib;
#ifdef DEBUGGING2
	fprintf(fout," %d : %d ",ib, icnt);
	if(h->extended) {
	  HistoDataType *data;
	  data = (h->indexdata)(h->bucket_data,ib);
	  (h->printdata)(fout,data);
	}
	fprintf(fout,"\n");
#endif
        }
    }
  }
#ifdef DEBUGGING1
  h->bucket_width = (h->hgh - h->low)/(h->nbucket) + 1;
  printf("numb,minb,maxb = %d,%d,%d\n",numb,minb,maxb);
  printf("h->low,h->bucket_width = %d,%d\n",
	 h->low,h->bucket_width);
#endif
#if 0
  cm = compute_cm(h, rez, numb);
  cs = (h->sync - h->low)/h->bucket_width;
  if (cs > 0) {
    cs = cs%cm - cm;
  } else {
    cs = - ( (-cs)%cm );
  }
  cs += ((h->nbucket - cs)/cm+1)*cm;
#else
  cm = 1;
  cs = h->nbucket;
#endif

#ifdef DEBUGGING1
  printf("cm,cs = %d,%d\n",cm,cs);
#endif

#if 0
  prec = compute_precision(h,minb,maxb);
  proc = compute_proc(h,cs,cm);
#endif

  single = (cm*h->bucket_width == 1);
  single = FALSE;
  if (single)
    { if (prec >= indent) indent = prec+1; }
  else
    { if (2*prec + 3 >= indent) indent = 2*prec+4; }

  if(h->extended) {
    assert(cm == 1);
  }

  first_flag = 1;
  {
    int ic;
    for (ic = 0; ic < cs; ic++) {
      // Accumulate the bucket information to also get percentiles from
      // the HIGHEST value.
      const int ib = cm*(cs - 1 - ic);
      int j;
      int sum_of_cnt = 0;

      int min_score = h->bucket_min[ib];
      int max_score = h->bucket_max[ib];

      min_score = MAX(min_score,h->min);
      max_score = MIN(max_score,h->max);
    
      for (j = 0; j < cm; j++) {
        if( (ib+j >= 0) && (ib+j < h->nbucket) )
          { 
            sum_of_cnt += h->bucket_cnt[ib+j];
            min_score = MIN(min_score,h->bucket_min[ib+j]);
            max_score = MAX(max_score,h->bucket_max[ib+j]);
            if(h->extended) {
              HistoDataType *data;
              data = (h->indexdata)(h->bucket_data,ib+j);
              if( j == 0 ) {
                (h->setdata)(h->temp_data,0,data);
              } else {
                (h->aggregate)(h->temp_data,0,data);
              }
            }
          }
      }
      if (sum_of_cnt > 0)
        {
          if(h->extended) {
            if(first_flag) {
              (h->setdata)(h->scan_data,0,h->temp_data);
              first_flag = 0;
            } else {
              (h->aggregate)(h->scan_data,0,h->temp_data);
            }
          }
          if(single || (min_score == max_score)) {
            fprintf(fout, "%*s%*d: %*d ",indent-prec,
                    "",prec,min_score,proc,sum_of_cnt);
            if(h->extended) {
              (h->printdata)(fout,h->temp_data,h->scan_data,h->aggr_data);
            }
            fprintf(fout,"\n");
          } else {
            fprintf(fout,"%*s%*d - %*d: %*d ",
                    indent-2*prec-3,"",
                    prec,min_score,prec,max_score,proc,sum_of_cnt);
            if(h->extended) {
              (h->printdata)(fout,h->temp_data,h->scan_data,h->aggr_data);
            }
            fprintf(fout,"\n");
          }
        }
    }
  }
}


#if 0
 h->cnt += 1;
 h->sum += data;
 h->mom += data*data;
 if( (h->cnt == 1) || (h->min > data))
   { h->min = data;}
 if( (h->cnt == 1) || (h->max < data))
   { h->max = data;}

/* Return the average of the data points in histogram "h" */

double histogram_avg(Histogram_t *histogram)
{ 
  HISTOGRAM *h = histogram;
  if (h->cnt == 0)
    return (0.);
  else
    return ((1.*h->sum)/h->cnt);
}

/* Return the standard deviation of the data points in histogram "h" */

double histogram_stdev(Histogram_t *histogram)
{ 
  HISTOGRAM *h = histogram;
  double fmom, fsum, fcnt;

  fmom = h->mom;
  fsum = h->sum;
  fcnt = h->cnt;
  if (h->cnt == 0)
    return (0.);
  else
    return (sqrt((fcnt*fmom - fsum*fsum)/(fcnt*fcnt)));
}
#endif



#if 0
  {
    /* Pretty up something */
    int bck,idiff;
    double nrm;
    bck = (hgh-low)/(h->nbucket-20) + 1;
    nrm = pow(10.,floor(log10((double)bck)));
    if (bck/nrm < 1.001)
      bck = nrm;
    else if (bck/nrm < 2.)
      bck = 2.*nrm;
    else if (bck/nrm < 5.)
      bck = 5.*nrm;
    else
      bck = 10.*nrm;
    h->bucket_width = bck;
    low = low - 10*bck;
    
    idiff = h->sync - low;
    if (idiff < 0) {
      h->low = low - (-idiff)%(h->bucket_width);
    } else {
      h->low = low + ( idiff)%(h->bucket_width);
    }
  }
#endif /* 0 */
