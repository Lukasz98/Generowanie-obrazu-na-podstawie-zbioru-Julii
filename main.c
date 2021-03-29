#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <complex.h>
#include <pthread.h>

#ifndef SCNu8
#define SCNu8 "hhu"
#endif
 

typedef struct
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
}
pixel_t;

typedef struct
{
    pixel_t *pixels;
    size_t width;
    size_t height;
}
bitmap_t;

struct Args {
    unsigned int W, H, N;
    long double xmin, xmax, ymin, ymax;
    long double cre, cim;
    long double xDiff, yDiff;
    long double complex c;
    unsigned int tcount, tId;
    bitmap_t bitmap;
    unsigned char * pallete;
};

void *calc(void * varg);
void createPallete(unsigned char * buff, int N, unsigned char * colors, int colorsCount);
void savePng(bitmap_t * bitmap, const char * path);

int main ()
{
    struct Args arg0;
    printf("W: ");
    scanf("%u", &arg0.W);
    printf("H: ");
    scanf("%u", &arg0.H);
    printf("N: ");
    scanf("%u", &arg0.N);
    printf("cre: ");
    scanf("%Lf", &arg0.cre);
    printf("cim: ");
    scanf("%Lf", &arg0.cim);
    printf("xmin: ");
    scanf("%Lf", &arg0.xmin);
    printf("xmax: ");
    scanf("%Lf", &arg0.xmax);
    printf("ymin: ");
    scanf("%Lf", &arg0.ymin);
    printf("ymax: ");
    scanf("%Lf", &arg0.ymax);
    printf("Liczba watkow: ");
    scanf("%u", &arg0.tcount);

    if (arg0.tcount == 0) {
        printf("Error threads = 0\n");
        return -1;
    }

    char path[201];
    printf("Sciezka do pliku (max 200 znakow, bez spacji): ");
    scanf("%200s", path);

    while (1) { // sprawdzenie czy plik istnieje
        FILE *file;
        if ((file = fopen(path,"r")) != NULL) { // plik juz istnieje
            fclose(file);
            printf("Ten plik juz istnieje. Nadpisac go? (t/n): ");
            char inp = 't';
            scanf(" %c", &inp);
            if (inp == 't')
                break;
            else {
                printf("Sciezka do pliku (max 200 znakow, bez spacji): ");
                scanf("%200s", path);
                continue;
            }
        }
        else { // taki plik nie istnieje
            break;
        }
    }

    arg0.xDiff = (arg0.xmax - arg0.xmin) / (arg0.W - 1);
    arg0.yDiff = (arg0.ymax - arg0.ymin) / (arg0.H - 1);
    arg0.c = arg0.cre + arg0.cim * I;

    arg0.bitmap.width = arg0.W;
    arg0.bitmap.height = arg0.H;

    arg0.bitmap.pixels = calloc(arg0.bitmap.width * arg0.bitmap.height, sizeof(pixel_t));

    if (!arg0.bitmap.pixels) {
	    printf("calloc error\n");
        return -1;
    }

    unsigned int colorsCount = 0;
    printf("Liczba kolorow glownych w palecie ( > 1): ");
    scanf("%d", &colorsCount);
    if (arg0.N < colorsCount) {
        printf("Zalozylem ze N musi byc >= od liczby kolorow\n");
        free(arg0.bitmap.pixels);
        return -1;
    }
    else if (colorsCount <= 1) {
        printf("Liczba kolorow <= 1\n");
        free(arg0.bitmap.pixels);
        return -1;
    }

    printf("Podaj kolory glowne w formacie: R G B R G B R G B ... : ");
    unsigned char colors[3 * colorsCount];
    for (unsigned int i = 0; i < colorsCount; ++i) {
        scanf("%" SCNu8, &colors[i * 3 + 0]);
        scanf("%" SCNu8, &colors[i * 3 + 1]);
        scanf("%" SCNu8, &colors[i * 3 + 2]);
    }

    int palleteSize = arg0.N;
    arg0.pallete = calloc(palleteSize, sizeof(pixel_t));
    if (!arg0.pallete) {
        free(arg0.bitmap.pixels);
        return -1;
    }
   
    createPallete(arg0.pallete, palleteSize, colors, colorsCount);

    pthread_t tid[arg0.tcount];
    struct Args args[arg0.tcount];
    
    for (unsigned int i = 0; i < arg0.tcount; ++i) {
        args[i] = arg0;
        args[i].tId = i;
    }

    printf("Rozpoczynam obliczanie\n");
    for (unsigned int i = 0; i < arg0.tcount; ++i) {
        pthread_create(&tid[i], NULL, calc, &args[i]);
    }

    for (unsigned int i = 0; i < arg0.tcount; ++i) {
        pthread_join(tid[i], NULL);
    }

    printf("Zapisuje do pliku...");
    savePng(&arg0.bitmap, path);
    printf("\n");
    
    free(arg0.bitmap.pixels);
    free(arg0.pallete);

    return 0;
}

void *calc(void * varg)
{
    struct Args * arg = (struct Args *)varg;

    for (unsigned int y = arg->tId; y <= arg->H - 1; y += arg->tcount) {
        long double xpos = arg->xmin;
        long double ypos = arg->ymin + arg->yDiff * (arg->H - 1 - y);
        unsigned int index = y * arg->W;
        for (unsigned int x = 0; x < arg->W; ++x) {
            long double complex p = xpos + ypos * I;
            long double complex z = p;
            unsigned int n = 1;
            while (n <= arg->N && cabsl(z) < 2.0) {
                z = z * z + arg->c;
                ++n;
            }
            if (n >= arg->N) { 
                n = arg->N - 1;
            }
            arg->bitmap.pixels[index + x].red = arg->pallete[n * 3 + 0];
            arg->bitmap.pixels[index + x].green = arg->pallete[n * 3 + 1];
            arg->bitmap.pixels[index + x].blue  = arg->pallete[n * 3 + 2];
            xpos += arg->xDiff;
        }
    }
    return NULL;
}



void createPallete(unsigned char * buff, int N, unsigned char * colors, int colorsCount)
{
    typedef struct  {
        float red, green, blue;
    } fcolor;
    int transitions = colorsCount - 1; // liczba przejsc z jednego koloru do drugiego
    int transitionLen = (N - 1) / transitions; // ile roznych kolorow jest pomiedzy 2 kolorami
    int transitionLenMod = (N - 1) % transitions; 
    int inputIndex = 0; 
    int index = 0;

    fcolor color = {colors[inputIndex * 3 + 0], colors[inputIndex * 3 + 1], colors[inputIndex * 3 + 2]};
    buff[index * 3 + 0] = (int)(color.red + 0.5f);
    buff[index * 3 + 1] = (int)(color.green + 0.5f);
    buff[index * 3 + 2] = (int)(color.blue + 0.5f);
    ++index;
    for (int j = 0; j < transitions; ++j) {
        fcolor nextC = {colors[(inputIndex + 1) * 3 + 0], colors[(inputIndex + 1) * 3 + 1], colors[(inputIndex + 1) * 3 + 2]};
        fcolor transition = {nextC.red - color.red, nextC.green - color.green, nextC.blue - color.blue};
        transition.red /= transitionLen;
        transition.green /= transitionLen;
        transition.blue /= transitionLen;
        for (int i = 0; i < transitionLen; ++i) {
            color.red += transition.red;
            color.green += transition.green;
            color.blue += transition.blue;
            buff[index * 3 + 0] = (int)(color.red + 0.5f);
            buff[index * 3 + 1] = (int)(color.green + 0.5f);
            buff[index * 3 + 2] = (int)(color.blue + 0.5f);
            ++index;
        }
        if (transitionLenMod > 0) {
            --transitionLenMod;
            buff[index * 3 + 0] = (int)(color.red + 0.5f);
            buff[index * 3 + 1] = (int)(color.green + 0.5f);
            buff[index * 3 + 2] = (int)(color.blue + 0.5f);
            ++index;
        }
        ++inputIndex;
    }
}

void savePng(bitmap_t * bitmap, const char * path)
{
    FILE * file = fopen(path, "wb");
    
    if (!file) {
        printf("Cannot open file: %s\n", path);
        return;
    }

    png_structp pngPtr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (pngPtr == NULL) {
        printf("Error while creating png write struct\n");
        fclose(file);
        return;
    }
    
    png_infop infoPtr = png_create_info_struct(pngPtr);
    if (infoPtr == NULL) {
        printf("Error while creating png info struct\n");
        fclose(file);
        png_destroy_write_struct(&pngPtr, &infoPtr);
        return;
    }
    
    /* domyslna metoda obslugi bledow, zalecana przez biblioteke libpng */
    if (setjmp(png_jmpbuf(pngPtr))) {
        printf("Error");
        fclose(file);
        png_destroy_write_struct(&pngPtr, &infoPtr);
        return;
    }
    
    int pixelSize = 3;
    int depth = 8;

    /* ustawienie atrybutow obrazu */
    png_set_IHDR(pngPtr, infoPtr, bitmap->width, bitmap->height, depth, PNG_COLOR_TYPE_RGB,
                  PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    

    png_byte ** rowPointers = png_malloc(pngPtr, bitmap->height * sizeof(png_byte*));
    for (size_t y = 0; y < bitmap->height; ++y) {
        png_byte *row = png_malloc(pngPtr, sizeof(uint8_t) * bitmap->width * pixelSize);
        rowPointers[y] = row;
        for (size_t x = 0; x < bitmap->width; ++x) {
            pixel_t * pixel = bitmap->pixels + bitmap->width * y + x; 
            *row++ = pixel->red;
            *row++ = pixel->green;
            *row++ = pixel->blue;
        }
    }
   
    /* Zapis do pliku */
    png_init_io(pngPtr, file);
    png_set_rows(pngPtr, infoPtr, rowPointers);
    png_write_png(pngPtr, infoPtr, PNG_TRANSFORM_IDENTITY, NULL);

    for (size_t y = 0; y < bitmap->height; ++y) {
        png_free(pngPtr, rowPointers[y]);
    }
    png_free(pngPtr, rowPointers);
    
    png_destroy_write_struct (&pngPtr, &infoPtr);
    fclose(file);
}


