#ifndef __BM3D_H__
#define __BM3D_H__

#include <iostream>
#include <time.h>
#include <omp.h>

#include "global_define.h"
#include "patch_2d.h"
#include "group_3d.h"

// 8x8 Kaiser window
extern const PatchType Kaiser[64];

/* Implementation of the first step, i.e. Hard-thresholding, of the BM3D denoising method for graysacle images.
 * Note that the implementation only supports graysacle images, or YUV 4:0:0 format.
 * If given the sigma of RGB colr space, which is usually thought as the same for R/G/B component,
 * the sigma of Y is relative to the transform matrix from RGB sapce to the YUV space,
 * For example, if Y = a*R + b*G + c*B, we can compute that sigmaY = sqrt(a*a + b*b + c*c) * sigmaR/G/B.
 * We can approximately compute sigmaY = 0.6 * sigmaR/G/B for the BT.601 or BT.709.
 */
class BM3D
{
public:
	BM3D();
	virtual ~BM3D();

	/* Load a new grayscale image and reset the buffers. */
	virtual void load(
		ImageType *org_noisy,		// pointer of the input noisy grayscale image
		int sigma,					// sigma of the Y component
		DistType max_mdist = 2500,	// maximum mean distance (L2/L1) between the reference and the candidate
		int sigmau = -1,			// sigma of the U component, has no use for YUV 4:0:0
		int sigmav = -1				// sigma of the V component, has no use for YUV 4:0:0
	);

	/* reset the buffers and redirect the processing reference patch to the first one of the noisy image */
	virtual void reset();

	/* Denoise just a line of reference patches and write out the completed rows. */
	virtual int next_line(
		ImageType *clean			// pointer of the output denoised grayscale image
	);

	/* Denoise a whole grayscale image and write out the result. */
	void run(
		ImageType *clean			// pointer of the output denoised grayscale image
	);

	/* grouping step of a single patch */
	void grouping();

	/* filtering step of a single patch */
	void filtering();

	/* aggregation step of a single patch */
	void aggregation();

	/* discard the first (pstep) rows and shift the remains of the numerator/denominator buffer */
	void shift_numer_denom();

protected:
	ImageType *noisy;	// padded noisy image

	Group3D *g3d;		// 3d group containg the reference patch and all its similar ones
	ImageType *refer;	// reference patch pointer (top-left)

	int row_cnt;		// counter of the processed rows of the original image

	PatchType *numerator;		// size: w * (2 * BM3D_SWINRV + BM3D_PSIZE)
	PatchType *denominator;		// size: w * (2 * BM3D_SWINRV + BM3D_PSIZE)
	PatchType *numer;			// template pointer
	PatchType *denom;			// template pointer

	DistType *dist_buf;		// sliding buffer to record the distances step by step
	DistType *dist_sum;		// distances buffer of each candidate patch

	int ncnt;				// counter of the steps

	clock_t gtime;			// timer of the grouping step
	clock_t ftime;			// timer of the filtering step
	clock_t atime;			// timer of the aggregation step
};

#endif

