#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "./images.h"

#define width1 WIDTH1
#define height1 HEIGTH1

uint16_t hist[256]; // declaracao global do histograma
// funcao de teste de logica pro histograma em CPP
// uint16_t EightBitHistogramTest(uint16_t width, uint16_t height, uint8_t *p_image, uint16_t *p_hist)
// chamada externa para a funcao que cria histograma em Assembly
extern uint16_t EightBitHistogram(uint16_t width, uint16_t height, const uint8_t *p_image, uint16_t *p_hist);

int main() {
    uint16_t imageSize = EightBitHistogram(width1, height1, *image1, &hist[0]);
    // display de cada valor do vetor do histograma calculado
    for (int i = 0; i< 256; ++i) {
        printf("%d\t%d\n", i, hist[i]);       
    }
    printf("Tamanho da imagem: %d", imageSize);
    return 0;
}

// funcao escrita em CPP usada para comparar resultados
// uint16_t EightBitHistogramTest(uint16_t width, uint16_t height, uint8_t *p_image, uint16_t *p_hist) {
//     uint16_t image_size = width * height;
//     if (image_size > 65536) {
//         std::cout << "Image too large!" << std::endl;
//         return 0;
//     } else {
//         for (int i = 0; i < height; i++) {
//           for(int j = 0; j < width; j++) {
//               ++p_hist[testImage[i][j]];
//           }
//         }
//     }
//     return image_size;
// }