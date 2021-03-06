#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "utils.h"
#include "../error/error.h"

//------------ METHODS FOR COLORSPACE CONVERSION ------------
// function that make the coversion from RGB to YUV
void rgb2yuv (png_img* img) {
	int i, j;
	int y, u, v, r, g, b;
	png_byte* row;
	png_byte* tmp;

	for (j=0; j<img->height; ++j) {
		row = img->data[j];

		for (i=0; i<img->width; ++i) {
			tmp = &(row[i*3]);

			// extract RGB values
			r = tmp[0];
			g = tmp[1];
			b = tmp[2];

			// convert pixel elements
			y = limit(0 + 0.299*r + 0.587*g + 0.114*b);
			u = limit(128 - 0.168736*r - 0.331264*g + 0.5*b);
			v = limit(128 + 0.5*r - 0.418688*g - 0.081312*b);

			// write back YUV values
			tmp[0] = y;
			tmp[1] = u;
			tmp[2] = v;
		}
	}
}

// function that make the coversion from YUV to RGB
void yuv2rgb (png_img* img) {
	int i, j;
	int y, u, v, r, g, b;
	png_byte* row;
	png_byte* tmp;

	for (j=0; j<img->height; ++j) {
		row = img->data[j];

		for (i=0; i<img->width; ++i) {
			tmp = &(row[i*3]);

			// extract YUV values
			y = tmp[0];
			u = tmp[1] - 128;
			v = tmp[2] - 128;
			
			// convert pixel elements
			r = limit(y + 1.402*v);
			g = limit(y - 0.3441*u - 0.7141*v);
			b = limit(y + 1.772*u);
			
			// write back YUV values
			tmp[0] = r;
			tmp[1] = g;
			tmp[2] = b;
		}
	}
}

// function that determines the signal-to-noise ratio between two images
double get_snr (png_img* img, png_img* ref) {
	double mse = 0.0;
	double snr = 0.0;
	png_byte* img_row;
	png_byte* img_pix;
	png_byte* ref_row;
	png_byte* ref_pix;
	int i, j, k;

	for (k=0; k<3; ++k) {
		for (j=0; j<img->height; ++j) {
			img_row = img->data[j];
			ref_row = ref->data[j];

			for (i=0; i<img->width; ++i) {
				img_pix = &(img_row[i*3]);
				ref_pix = &(ref_row[i*3]);

				mse += pow ((img_pix[k] - ref_pix[k]), 2);
			}
		}
	}

	mse = (1.0/(img->width*img->height*3.0)) * mse;
	snr = 20 * log10 (255.0/sqrt(mse));

	return snr;
}



//--------------------- DCT CALCULATIONS --------------------
void dct_1d (int const len, double arr[len]) {
	int i, j;
	double sum = 0.0;
	double tmp[len];

	for (j=0; j<len; ++j) {
		for (i=0; i<len; ++i) {
			sum += arr[i] * (cos((PI/len)*(i+0.5)*j));
		}
		tmp[j] = sum;
		sum = 0.0;
	}
}

void dct_2d (int const len, double arr[len][len]) {
	int i, j, k, l;
	double sum, ai, aj;			// multiplicative constants
	double tmp[len][len];		// dct-value buffer
	double fac;						// result of the cosine multiplication

	for (j=0; j<len; ++j) {
		for (i=0; i<len; ++i) {
			ai = (i == 0) ? 1.0 / sqrt((double)len) : sqrt(2.0/(double)len);
			aj = (j == 0) ? 1.0 / sqrt((double)len) : sqrt(2.0/(double)len);

			for (l=0; l<len; ++l) {
				for (k=0; k<len; ++k) {
					fac = cos((PI/(double)len) * ((double)l + 0.5) * (double)j) * cos((PI/(double)len) * ((double)k + 0.5) * (double)i);
					sum += arr[l][k] * fac;
				}
			}
			tmp[j][i] = ai * aj * sum;
			sum = 0.0;
		}
	}

	// write back to original matrix
	for (j=0; j<len; ++j) {
		for (i=0; i<len; ++i) {
			arr[j][i] = tmp[j][i];
		}
	}
}

void dct_3d (int const len, int const z, double arr[z][len][len]) {
	int i, j, k, l, m, n;
	double sum, ai, aj, ak;
	double tmp[z][len][len];		// dct result
	double fac;

	for (k=0; k<z; ++k) {
		for (j=0; j<len; ++j) {
			for (i=0; i<len; ++i) {
				ai = (i == 0) ? 1.0/sqrt((double)len) : sqrt(2.0/(double)len);
				aj = (j == 0) ? 1.0/sqrt((double)len) : sqrt(2.0/(double)len);
				ak = (k == 0) ? 1.0/sqrt((double)z) : sqrt(2.0/(double)z);

				for (n=0; n<z; ++n) {
					for (m=0; m<len; ++m) {
						for (l=0; l<len; ++l) {
							fac = cos((PI/(double)z)*((double)n+0.5)*(double)k) * cos((PI/(double)len)*((double)m+0.5)*(double)j) * cos((PI/(double)len)*((double)l+0.5)*(double)i);
							sum += arr[n][m][l] * fac;
						}
					}
				}
				tmp[k][j][i] = ai * aj * ak * sum;
				sum = 0.0;
			}
		}
	}

	// write back to original matrix
	for (k=0; k<z; ++k) {
		for (j=0; j<len; ++j) {
			for (i=0; i<len; ++i) {
				arr[k][j][i] = tmp[k][j][i];
			}
		}
	}
}

void idct_3d (int const len, int const z, double arr[z][len][len]) {
	int i, j, k, l, m, n;
	double sum, al, am, an = 0.0;
	double tmp[z][len][len];		// dct result
	double fac;

	for (k=0; k<z; ++k) {
		for (j=0; j<len; ++j) {
			for (i=0; i<len; ++i) {
				for (n=0; n<z; ++n) {
					for (m=0; m<len; ++m) {
						for (l=0; l<len; ++l) {
							al = (l == 0) ? 1.0/sqrt((double)len) : sqrt(2.0/(double)len);
							am = (m == 0) ? 1.0/sqrt((double)len) : sqrt(2.0/(double)len);
							an = (n == 0) ? 1.0/sqrt((double)z) : sqrt(2.0/(double)z);

							fac = cos((PI/(double)len)*((double)j+0.5)*(double)m) * cos((PI/(double)len)*((double)i+0.5)*(double)l) * cos((PI/(double)z)*((double)k+0.5)*(double)n);
							sum += arr[n][m][l] * al * am * an * fac;
						}
					}
				}
				tmp[k][j][i] = sum;
				// if (k==0 && j==0 && i==0) printf ("sum: %f\n", sum);
				// if (k==0 && j==0 && i==0) printf ("ai: %f\naj: %f\nak: %f\n", ai, aj, ak);
				// if (k==0 && j==0 && i==0) printf ("product: %f\n", tmp[k][j][i]);
				sum = 0.0;
			}
		}
	}

	// write back to original matrix
	for (k=0; k<z; ++k) {
		for (j=0; j<len; ++j) {
			for (i=0; i<len; ++i) {
				arr[k][j][i] = tmp[k][j][i];
			}
		}
	}
}


// limits a given value to 255
int limit (int const x) {
	return (x < 0) ? 0 : (x > 255) ? 255 : x;
}

// removes the extension from a given filename
int exclude_extension (char* const str, char* name) {
	int count = 0;
	char tmp[30];
	char* iter = 0;
	
	iter = strrchr (str, '_');

	if (!iter) {
		iter = strrchr (str, '/');
	}

	if (!iter) {
		generate_error ("Invalid input filename...");
		return 1;
	}

	++iter; 		// in order to exclude the '/' itself as well

	while (iter && (*iter != '.') && (*iter != '[')) {
		tmp[count] = *iter;
		++iter;
		++count;
	}

	tmp[count] = '\0';
	count = sprintf (name, "%s", tmp);

	return (count == 0) ? 1 : 0;
}

// produces an output filename from given string literals
int get_output_filename (char* outfile, char* const path, char* const prefix, char* const ext, int const attr) {
	int count;

	if (attr != 0) {
		if (attr < 100) {
			count = sprintf (outfile, "%s%s[0%d].%s", path, prefix, attr, ext); 
		}
		else {
			count = sprintf (outfile, "%s%s[%d].%s", path, prefix, attr, ext);
		}
	}
	else {
		count = sprintf (outfile, "%s%s.%s", path, prefix, ext); 
	}

	return (count == 0) ? 1 : 0;
}
