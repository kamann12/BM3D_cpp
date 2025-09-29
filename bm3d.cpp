#include <iostream>
#include "bm3d.h"

#if USE_INTEGER
const PatchType Kaiser[64] = {
	3,	5,	6,  7,  7,  6,  5, 3,
	5,	7,  9, 10, 10,  9,  7, 5,
	6,  9, 12, 13, 13, 12,  9, 6,
	7, 10, 13, 15, 15, 13, 10, 7,
	7, 10, 13, 15, 15, 13, 10, 7,
	6,  9, 12, 13, 13, 12,  9, 6,
	5,  7,  9, 10, 10,  9,  7, 5,
	3,  5,  6,  7,  7,  6,  5, 3 
};
#else
const PatchType Kaiser[64] = {
	0.1924f, 0.2989f, 0.3846f, 0.4325f, 0.4325f, 0.3845f, 0.2989f, 0.1924f,
	0.2989f, 0.4642f, 0.5974f, 0.6717f, 0.6717f, 0.5974f, 0.4642f, 0.2989f,
	0.3846f, 0.5974f, 0.7688f, 0.8644f, 0.8644f, 0.7689f, 0.5974f, 0.3846f,
	0.4325f, 0.6717f, 0.8644f, 0.9718f, 0.9718f, 0.8644f, 0.6717f, 0.4325f,
	0.4325f, 0.6717f, 0.8644f, 0.9718f, 0.9718f, 0.8644f, 0.6717f, 0.4325f,
	0.3846f, 0.5974f, 0.7688f, 0.8644f, 0.8644f, 0.7689f, 0.5974f, 0.3846f,
	0.2989f, 0.4642f, 0.5974f, 0.6717f, 0.6717f, 0.5974f, 0.4642f, 0.2989f,
	0.1924f, 0.2989f, 0.3846f, 0.4325f, 0.4325f, 0.3845f, 0.2989f, 0.1924f
};
#endif

static inline DistType get_dist(ImageType a, ImageType b)
{
	int diff = (int)a - b;
#if USE_L2_DIST
	return diff * diff;
#else
	return diff >= 0 ? diff : -diff;
#endif
}

BM3D::BM3D()
{
    g3d   = new Group3D(BM3D_PSIZE, BM3D_PSIZE, BM3D_MAX_SIM);
    noisy = new ImageType[BM3D_W * BM3D_H]();

    numerator   = new PatchType[BM3D_W * (BM3D_PSIZE + BM3D_SWINRV * 2)];
    denominator = new PatchType[BM3D_W * (BM3D_PSIZE + BM3D_SWINRV * 2)];

    dist_buf = new DistType[BM3D_NSH * BM3D_NSV * BM3D_NBUF];
    dist_sum = new DistType[BM3D_NSH * BM3D_NSV];

    row_cnt = BM3D_H;	// avoid processing without the noisy image initialization
}

BM3D::~BM3D()
{
	delete g3d;
	delete[] noisy;
	delete[] numerator;
	delete[] denominator;
	delete[] dist_buf;
	delete[] dist_sum;
}

void BM3D::run(ImageType *clean)
{
	gtime = 0;
	ftime = 0;
	atime = 0;

	if (row_cnt > 0) 
		reset();
	while (next_line(clean) >= 0);

	clock_t stime = gtime + ftime + atime;
	std::cout << "time(s): " 
			  << (double)gtime / CLOCKS_PER_SEC << ' ' 
			  << (double)ftime / CLOCKS_PER_SEC << ' ' 
			  << (double)atime / CLOCKS_PER_SEC << ' ' 
			  << (double)stime / CLOCKS_PER_SEC << std::endl;

	std::cout << "percentage: " 
			  << (double)gtime / stime * 100 << ' ' 
			  << (double)ftime / stime * 100 << ' ' 
			  << (double)atime / stime * 100 << std::endl;
}

void BM3D::reset()
{
    row_cnt = 0;
    memset(numerator,   0, (BM3D_PSIZE + BM3D_SWINRV * 2) * BM3D_W * sizeof(PatchType));
    memset(denominator, 0, (BM3D_PSIZE + BM3D_SWINRV * 2) * BM3D_W * sizeof(PatchType));
}

void BM3D::load(ImageType *org_noisy, int sigma, DistType max_mdist, int sigmau, int sigmav)
{
    row_cnt = 0;
    g3d->set_thresholds(sigma, max_mdist * BM3D_PSIZE * BM3D_PSIZE);

    ImageType *tmp_noisy = noisy + BM3D_SWINRV * BM3D_W + BM3D_SWINRH;
    for (int i = 0; i < BM3D_ORIG_H; i++)
    {
        memcpy(tmp_noisy, org_noisy, BM3D_ORIG_W * sizeof(ImageType));
        for (int j = 0; j < BM3D_W_PAD; j++)
        {
            // edge padding
            tmp_noisy[BM3D_ORIG_W + j] = tmp_noisy[BM3D_ORIG_W + j - 1];
        }
        tmp_noisy += BM3D_W;
        org_noisy += BM3D_ORIG_W;
    }
    for (int i = 0; i < BM3D_H_PAD; i++)
    {
        memcpy(tmp_noisy, tmp_noisy - BM3D_W, (BM3D_ORIG_W + BM3D_W_PAD) * sizeof(ImageType));
        tmp_noisy += BM3D_W;
    }
    memset(numerator,   0, (BM3D_PSIZE + BM3D_SWINRV * 2) * BM3D_W * sizeof(PatchType));
    memset(denominator, 0, (BM3D_PSIZE + BM3D_SWINRV * 2) * BM3D_W * sizeof(PatchType));
}

/* Porcess a line of reference patches, the location of the line is recorded by (this->row_cnt).
 * The distances buffers used in the last reference patches line will be reset in the beginning, 
 * meaning that the grouping process of each reference patches line is independent.
 * When all reference patches in the line are processed, as we will step downward to next line, 
 * the beginning (this->pstep) rows of the buffers (numerator and denominator) will no longer be modified,
 * and we can write out the result to the clean (denoised) image.
 * Note that the number of rows we can write out is different in the beginning or the end, 
 * the function will return how exactly many rows we can write out.
 * The function is unidirectional that, you must call the function one by one 
 * so that the (this->row_cnt) increases step by step from 0 to the end of the image, 
 * as the numerator/denominator buffer records only partial information and updates progressively.
 */
int BM3D::next_line(ImageType *clean)
{
    if (row_cnt >= BM3D_ORIG_H + BM3D_PSTEP - BM3D_PSIZE) return -1;	// beyond the last line of reference patches

    refer = noisy + (row_cnt + BM3D_SWINRV) * BM3D_W + BM3D_SWINRH;	// the first reference patch of the line
    numer = numerator   + BM3D_SWINRV * BM3D_W + BM3D_SWINRH;
    denom = denominator + BM3D_SWINRV * BM3D_W + BM3D_SWINRH;

    memset(dist_buf, 0, BM3D_NSH * BM3D_NSV * BM3D_NBUF * sizeof(DistType));
    memset(dist_sum, 0, BM3D_NSH * BM3D_NSV * sizeof(DistType));

    // initialize the distance buffer
    #pragma omp parallel for num_threads(USE_THREADS_NUM)
    for (int sy = -BM3D_SWINRV; sy <= BM3D_SWINRV; sy += BM3D_SSTEPV)
    {
        for (int sx = -BM3D_SWINRH; sx <= BM3D_SWINRH; sx += BM3D_SSTEPH)
        {
            int idx = (sy + BM3D_SWINRV) / BM3D_SSTEPV * BM3D_NSH + (sx + BM3D_SWINRH) / BM3D_SSTEPH;
            for (int y = 0; y < BM3D_PSIZE; y++)
            {
                for (int x = 0; x < BM3D_PSIZE - BM3D_PSTEP; x++)
                {
                    dist_buf[BM3D_NBUF * idx + x / BM3D_PSTEP] += get_dist(refer[y * BM3D_W + x], refer[(y + sy) * BM3D_W + x + sx]);
                }
            }
            for (int i = 0; i < BM3D_NBUF - 2; i++) {
                dist_sum[idx] += dist_buf[BM3D_NBUF * idx + i];
            }
        }
    }
    ncnt = BM3D_NBUF;

    clock_t t;
    // proceesing the line
    for (int x = 0; x < BM3D_ORIG_W + BM3D_PSTEP - BM3D_PSIZE; x += BM3D_PSTEP, ncnt++)
    {
        t = clock();
        grouping();
        gtime += clock() - t;

        t = clock();
        filtering();
        ftime += clock() - t;

        t = clock();
        aggregation();
        atime += clock() - t;

        refer += BM3D_PSTEP;
        numer += BM3D_PSTEP;
        denom += BM3D_PSTEP;
    }

    // output the completed rows
    numer = numerator   + BM3D_SWINRH;
    denom = denominator + BM3D_SWINRH;

    int output_rows;
    if (row_cnt < BM3D_SWINRV) 
    {
        if (row_cnt + BM3D_PSTEP <= BM3D_SWINRV) {
            // no row is completed in the begining
            output_rows = 0;
        }
        else {
            output_rows = row_cnt + BM3D_PSTEP - BM3D_SWINRV;
            numer += (BM3D_PSTEP - output_rows) * BM3D_W;
            denom += (BM3D_PSTEP - output_rows) * BM3D_W;
        }
    } 
    else 
    {
        if (row_cnt >= BM3D_ORIG_H - BM3D_PSIZE)
            output_rows = BM3D_ORIG_H - row_cnt + BM3D_SWINRV;	// the last line of reference patches
        else
            output_rows = BM3D_PSTEP;
        clean += BM3D_ORIG_W * (row_cnt - BM3D_SWINRV);
    }

    for (int i = 0, r = 0; r < output_rows; r++)
    {
        for (int c = 0; c < BM3D_ORIG_W; c++, i++)
        {
            clean[i] = (ImageType)(numer[c] / denom[c]);
        }
        numer += BM3D_W;
        denom += BM3D_W;
    }

    // remove the fisrt (pstep) rows of the numerator and denominator buffers
    // and insert (pstep) new rows to the end of the buffers
    shift_numer_denom();

    row_cnt += BM3D_PSTEP;
    return output_rows;
}

void BM3D::grouping()
{
    #pragma omp parallel for num_threads(USE_THREADS_NUM)
    for (int sy = -BM3D_SWINRV; sy <= BM3D_SWINRV; sy += BM3D_SSTEPV)
    {
        for (int sx = -BM3D_SWINRH; sx <= BM3D_SWINRH; sx += BM3D_SSTEPH)
        {
            int idx = (sy + BM3D_SWINRV) / BM3D_SSTEPV * BM3D_NSH + (sx + BM3D_SWINRH) / BM3D_SSTEPH;
            for (int y = 0; y < BM3D_PSIZE; y++)
            {
                for (int x = BM3D_PSIZE - BM3D_PSTEP; x < BM3D_PSIZE; x++)
                {
                    dist_buf[BM3D_NBUF * idx + (ncnt + x / BM3D_PSTEP) % BM3D_NBUF] += get_dist(refer[y * BM3D_W + x], refer[(y + sy) * BM3D_W + x + sx]);
                }
            }
            dist_sum[idx] += dist_buf[BM3D_NBUF * idx + (ncnt - 2) % BM3D_NBUF];
            dist_sum[idx] += dist_buf[BM3D_NBUF * idx + (ncnt - 1) % BM3D_NBUF];
        }
    }

    g3d->set_reference();
    for (int idx = 0, sy = -BM3D_SWINRV; sy <= BM3D_SWINRV; sy += BM3D_SSTEPV)
    {
        for (int sx = -BM3D_SWINRH; sx <= BM3D_SWINRH; sx += BM3D_SSTEPH, idx++)
        {
            g3d->insert_patch(sx, sy, dist_sum[idx]);
        }
    }
    #pragma omp parallel for num_threads(USE_THREADS_NUM)
    for (int i = 0; i < BM3D_NSV; i++)
    {
        for (int j = 0; j < BM3D_NSH; j++)
        {
            int idx = i * BM3D_NSH + j;
            dist_sum[idx] -= dist_buf[BM3D_NBUF * idx + (ncnt - 1) % BM3D_NBUF];
            dist_sum[idx] -= dist_buf[BM3D_NBUF * idx + (ncnt - 0) % BM3D_NBUF];
            dist_buf[BM3D_NBUF * idx + ncnt % BM3D_NBUF] = 0;
        }
    }

    g3d->fill_patches_values(refer, BM3D_W);
}

void BM3D::filtering()
{
	g3d->transform_3d();
	g3d->hard_thresholding();
	g3d->inv_transform_3d();
}

void BM3D::aggregation()
{
    PatchType weight = g3d->get_weight();
    for (int p = 0; p < g3d->num; p++)
    {
        int x = g3d->patch[p]->x;
        int y = g3d->patch[p]->y;
        for (int i = 0, r = 0; r < BM3D_PSIZE; r++)
        {
            for (int c = 0; c < BM3D_PSIZE; c++, i++)
            {
                numer[(r + y) * BM3D_W + c + x] += Kaiser[i] * weight * g3d->patch[p]->values[i];
                denom[(r + y) * BM3D_W + c + x] += Kaiser[i] * weight;
            }
        }
    }
}

void BM3D::shift_numer_denom()
{
    numer = numerator;
    denom = denominator;
    for (int i = 0; i < 2 * BM3D_SWINRV + BM3D_PSIZE - BM3D_PSTEP; i++)
    {
        memcpy(numer, numer + BM3D_W * BM3D_PSTEP, BM3D_W * sizeof(PatchType));
        memcpy(denom, denom + BM3D_W * BM3D_PSTEP, BM3D_W * sizeof(PatchType));
        numer += BM3D_W;
        denom += BM3D_W;
    }
    memset(numer, 0, BM3D_W * BM3D_PSTEP * sizeof(PatchType));
    memset(denom, 0, BM3D_W * BM3D_PSTEP * sizeof(PatchType));
}