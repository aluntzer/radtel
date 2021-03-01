#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>


/* velocity bins */
#define VEL 801


static void import_spec(FILE *f, FILE *data)
{
	char *t;
	char line[256];

	short as[VEL];
	float a[VEL];
	int n[VEL];

	float tmp;

	int i;



	bzero(n, sizeof(int) * VEL);
	bzero(a, sizeof(float) * VEL);


	while (fgets(line, 256, f)) {

		t = strtok(line, "\t ");

		if (t) {
			if (t[0] == '%')
				continue; /* comment, go to next line */

			tmp = strtod(t, NULL);

			t = strtok(NULL, "\t ");
		}

		if (t) {

			i = (int) (400. + nearbyint(tmp));
			tmp = strtod(t, NULL);
			a[i] += tmp;
			n[i]++;
		}
	}

	for (i = 0; i < VEL; i++) {

		/* interpolate if there was no value for this velocity bin
		 * note that we presume that the shape of the data never
		 * allows two subsequent zero-bins
		 */

		if (!n[i]) {
			int i1, i2;

			i1 = i - 1;
			i2 = i + 1;

			if (i1 < 0)
				i1 = i;

			if (i2 > VEL)
				i2 = i;

			n[i] = 1;

			a[i] = (a[i1] * (float) n[i1]+ a[i2] * (float) n[i2]);
			a[i] = a[i] / ((float) n[i1] + (float)n[i2]);
		}

		a[i] /= (float) n[i];

		/* drop one digit, cK resolution will suffice */
		as[i] = (short) (a[i] * 100.);
	}

	fwrite(as, sizeof(short), 801, data);
}




void convert(void)
{
	FILE *f;

	char fname[256];

	double glon;
	double glat;



	FILE *data = fopen("vel_short_int.dat", "wb");



	for (glon = 0.0; glon <= 360.0; glon+=0.5) {

		for (glat = -90.0; glat <= 90.0; glat+=0.5) {

			fprintf(stderr, "offset %ld, pos %.1f, %.1f\n", ftell(data), glon, glat);

			snprintf(fname, sizeof(fname), "dl/%.1f_%.1f.txt", glon, glat);
			f = fopen(fname, "r");

			if (!f) {
				printf("%s: error opening file %s\n", __func__, fname);
			} else {
				import_spec(f, data);
				fclose(f);
			}

		}
	}

	fclose(data);
}


int main(void)
{
	convert();
	return 0;
}
