#include <iostream>
#include <string>
#include <math.h>
#include "cbm3d.h"
#include "cbm3d_wiener.h"
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
	int w = 160, h = 120;
	int chnl = 1;			// YUV 4:0:0 or 4:4:4

	int en_bm3d_step2 = 0;	// enable step2 of bm3d
	int sigma_step1 = 36;	// here same for Y/U/V, can be different
	int sigma_step2 = 25;	// bigger for smoother, usually a little smaller than step1

	int frames = 1;		// frames to process

	// noisy input and denoised output
	FILE *inf = openfile("test/in.yuv", "rb");
	FILE *ouf = openfile("test/out.yuv", "wb");
	
	ImageType *noisy = new ImageType[w * h * chnl];
	ImageType *clean = new ImageType[w * h * chnl];

	/* hard-thresholding denoiser
	 */ 
	BM3D *denoiser = NULL;
	denoiser = new BM3D(w, h, 16, 8, 3, 16, 1, 16, 1); // at present the psize must be 8

	int frame = 0;
	while (frames < 0 ||frame < frames)
	{
		if (fread(noisy, sizeof(ImageType), w * h * chnl, inf) != w * h * chnl) break;
		cout << "Processing frame " << frame << "..." << endl;

		// hard thresholding
		denoiser->load(noisy, sigma_step1);
		denoiser->run(clean);

		fwrite(clean, sizeof(ImageType), w * h * chnl, ouf);

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
