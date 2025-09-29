#include <iostream>
#include <string>
#include <math.h>
#include "bm3d.h"
using namespace std;

FILE *openfile(const char *fname, const char *mode)
{
	FILE *f = fopen(fname, mode);
	if (NULL == f)
	{
		cout << "Failed to open: " << fname << endl;
		exit(1);
	}
	return f;
}

int main()
{
	int sigma_step1 = 36;	// here same for Y/U/V, can be different
	int sigma_step2 = 25;	// bigger for smoother, usually a little smaller than step1

	int frames = 1;		// frames to process

	// noisy input and denoised output
	FILE *inf = openfile("test/in.yuv", "rb");
	FILE *ouf = openfile("test/out.yuv", "wb");
	
	printf("Width: %d, Height: %d\n", BM3D_ORIG_W, BM3D_ORIG_H);
	ImageType *noisy = new ImageType[BM3D_ORIG_W * BM3D_ORIG_H];
	ImageType *clean = new ImageType[BM3D_ORIG_W * BM3D_ORIG_H];

	/* hard-thresholding denoiser
	 */ 
	BM3D *denoiser = NULL;
	denoiser = new BM3D(); // パラメータは定数マクロで固定

	int frame = 0;
	while (frames < 0 || frame < frames)
	{
		if (fread(noisy, sizeof(ImageType), BM3D_ORIG_W * BM3D_ORIG_H, inf) != BM3D_ORIG_W * BM3D_ORIG_H) break;
		cout << "Processing frame " << frame << "..." << endl;

		// hard thresholding
		denoiser->load(noisy, sigma_step1);
		denoiser->run(clean);

		fwrite(clean, sizeof(ImageType), BM3D_ORIG_W * BM3D_ORIG_H, ouf);

		cout << "Frame " << frame << " done!" << endl << endl;
		frame++;
	}

	delete denoiser;

	fclose(inf);
	fclose(ouf);
	delete[] noisy;
	delete[] clean;

	return 0;
}
