#include <iostream>
#include <string>
#include <chrono>
#include <opencv2\opencv.hpp>
#include <opencv2\core.hpp>
#include <opencv2\highgui.hpp>
#include <opencv2\videoio.hpp>

extern "C" {
#include "vc.h"
}

int main(void) {
	// V�deo
	char videofile[20] = "video.avi";
	cv::VideoCapture capture; // capturar o vídeo da câmara
	struct
	{
		int width, height;
		int ntotalframes;
		int fps;
		int nframe;
	} video; // definição da estrutura do vídeo
	// Outros
	std::string str;
	int key = 0;

	/* Leitura de v�deo de um ficheiro */
	/* NOTA IMPORTANTE:
	O ficheiro video.avi dever� estar localizado no mesmo direct�rio que o ficheiro de c�digo fonte.
	*/
	//capture.open(videofile);

	/* Em alternativa, abrir captura de v�deo pela Webcam #0 */
	//capture.open(0, cv::CAP_DSHOW); // Pode-se utilizar apenas capture.open(0);
	capture.open(0);

	/* Verifica se foi poss�vel abrir o ficheiro de v�deo */
	if (!capture.isOpened())
	{
		std::cerr << "Erro ao abrir o ficheiro de v�deo!\n";
		return 1;
	}

	// obtem as propriedades do vídeo
	video.ntotalframes = (int)capture.get(cv::CAP_PROP_FRAME_COUNT); /* N�mero total de frames no v�deo */
	video.fps = (int)capture.get(cv::CAP_PROP_FPS); /* Frame rate do v�deo */
	/* Resolu��o do v�deo */
	video.width = (int)capture.get(cv::CAP_PROP_FRAME_WIDTH);
	video.height = (int)capture.get(cv::CAP_PROP_FRAME_HEIGHT);

	/* Cria uma janela para exibir o v�deo */
	cv::namedWindow("VC - VIDEO", cv::WINDOW_AUTOSIZE);

	cv::Mat frame; // armazenar cada frame individual do vídeo
	cv::Mat gb; // armazenar a imagem depois de GaussianBlur

	capture.read(frame);
	IVC* image = vc_image_new(video.width, video.height, 3, 255);
	IVC* imageHsv = vc_image_new(video.width, video.height, 3, 255); // armazenar a imagem convertida em HSV
	IVC* hsvSegAzul = vc_image_new(video.width, video.height, 1, 255); // armazenar o resultado da segmentação da cor azul
	IVC* hsvSegVermelho = vc_image_new(video.width, video.height, 1, 255); // armazenar o resultado da segmentação da cor vermelho

	int i;
	int azul, vermelho; // armazenar o número de píxeis na imagem a azul ou vermelho
	int nBlob; // armazenar o número total de blobs
	int contBlob; // contar o número de blobs que tem área maior que o limiar 
	int maxArea, maxArea2; // armazenar as áreas dos dois maiores blobs encontrados na imagem
	int area; // soma das áreas dos blobs que passaram pelo limiar
	int indSeta; // armazenar o índice do blob que representa a seta
	float seta; // calcular a posição da seta em relação ao centro do blob

	while (key != 'q') {
		IVC* imageBlob = vc_image_new(video.width, video.height, 1, 255); // armazenar os resultados da info de blobs
		IVC* image2 = vc_image_new(video.width, video.height, 3, 255);
		OVC* blobInfo; // armazenar informações sobre blobs de uma imagem
		/* Leitura de uma frame do v�deo */
		capture.read(frame);
		cv::GaussianBlur(frame, gb, cv::Size(5, 5), cv::BORDER_DEFAULT); // GaussianBlur para reduzir o ruído
		frame = gb;

		/* Verifica se conseguiu ler a frame */
		if (frame.empty()) break;

		contBlob = 0;
		maxArea = 0;
		maxArea2 = 0;
		area = 1;

		memcpy(image->data, frame.data, video.width * video.height * 3); // copia os dados de frame para image para a usar nas operações seguintes

		vc_bgr_to_hsv(image, imageHsv);

		azul = vc_hsv_segmentation_invert(imageHsv, hsvSegAzul, 200, 240, 60, 100, 40, 100); // segmentação dos píxeis a azul
		vermelho = vc_hsv_segmentation_invert(imageHsv, hsvSegVermelho, 340, 360, 50, 100, 50, 100); // segmentação dos píxeis a vermelho

		if (azul > 10000 || vermelho > 10000)
		{
			if (azul > vermelho)
			{
				blobInfo = vc_binary_blob_labelling(hsvSegAzul, imageBlob, &nBlob); // faz o blob labelling
				vc_binary_blob_info(imageBlob, blobInfo, nBlob); // obtém informações sobre cada blob

				// percorre os blobs identificados e encontra o que tem maior e segunda maior área
				// maior área: blob do fundo; segunda maior área: blob do sinal
				for (i = 0; i <= nBlob; i++) {
					if (blobInfo[i].area > maxArea) {
						maxArea2 = maxArea;
						maxArea = blobInfo[i].area;
					}
				}

				// escolha de blobs de interesse
				for (i = 0; i <= nBlob; i++) {
					if (blobInfo[i].area < maxArea && blobInfo[i].area >(azul * 0.01)) { // encontrar blobs menores que maxArea e maiores que 1% de azul
						if (blobInfo[i].area == maxArea2) {
							indSeta = i; // armazena o indice do blob
						}
						area += blobInfo[i].area; // soma a área dos blobs selecionados
						contBlob++; // contador de blobs
					}
				}

				float percentagem = (((float)area) / ((float)azul)) * 100; // percentagem de blobs brancos no azul

				vc_gray_to_rgb(hsvSegAzul, image);
				memcpy(frame.data, image->data, video.width * video.height * 3);

				if (percentagem > 32 && percentagem < 50) {

					vc_binary_invert(hsvSegAzul);

					if (contBlob >= 5 && contBlob <= 8)
					{
						
						str = std::string("AUTO-ESTRADA");
						cv::putText(frame, str, cv::Point(20, 75), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
						cv::putText(frame, str, cv::Point(20, 75), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
					}
					else
					{

						// repete o processo de identificar e obter informações dos blobs
						blobInfo = vc_binary_blob_labelling(hsvSegAzul, imageBlob, &nBlob); // faz o blob labelling
						vc_binary_blob_info(imageBlob, blobInfo, nBlob); // obtém informações sobre cada blob

						// repete o processo de verificar os maiores blobs
						maxArea = 0;
						for (i = 0; i <= nBlob; i++) {
							if (blobInfo[i].area > maxArea) {
								maxArea = blobInfo[i].area;
								indSeta = i;
							}
						}
						// diferença entre o centro do sinal e o centro do blob
						// quanto mais próximo de 0, mais perto do centro
						seta = (float)(blobInfo[indSeta].x + ((float)blobInfo[indSeta].width / 2)) - (float)(blobInfo[indSeta].xc);

						if (seta >= -3 && seta <= 3) {
							str = std::string("CARRO");
							cv::putText(frame, str, cv::Point(20, 75), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
							cv::putText(frame, str, cv::Point(20, 75), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
						}
						else
						{
							if (seta <= 0) { // menor que 0, situa-se à esquerda
								str = std::string("SETA ESQUERDA");
								cv::putText(frame, str, cv::Point(20, 75), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
								cv::putText(frame, str, cv::Point(20, 75), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
							}
							else
							{
								str = std::string("SETA DIREITA");
								cv::putText(frame, str, cv::Point(20, 75), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
								cv::putText(frame, str, cv::Point(20, 75), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
							}
						}
					}
				}
			}
			else {
				vc_gray_to_rgb(hsvSegVermelho, image2);
				memcpy(frame.data, image2->data, video.width * video.height * 3);

				// repete o processo de identificar e obter informações dos blobs
				blobInfo = vc_binary_blob_labelling(hsvSegVermelho, imageBlob, &nBlob); // faz o blob labelling
				vc_binary_blob_info(imageBlob, blobInfo, nBlob); // obtém informações sobre cada blob

				// procura o maior blob
				for (i = 0; i <= nBlob; i++) {
					if (blobInfo[i].area > maxArea) {
						maxArea = blobInfo[i].area;
					}
				}

				// escolha de blobs de interesse
				for (i = 0; i <= nBlob; i++) {
					if (blobInfo[i].area != maxArea && blobInfo[i].area > (vermelho * 0.01)) { // encontrar blobs que não são a maior área e maiores que 1% de vermelho
						contBlob++;
					}
				}

				vc_binary_invert(hsvSegVermelho);

				// repete o processo de identificar e obter informações dos blobs
				blobInfo = vc_binary_blob_labelling(hsvSegVermelho, imageBlob, &nBlob);
				vc_binary_blob_info(imageBlob, blobInfo, nBlob);

				// repete o processo de verificar os maiores blobs
				maxArea = 0;
				for (i = 0; i <= nBlob; i++) {
					if (blobInfo[i].area > maxArea) {
						maxArea = blobInfo[i].area;
					}
				}

				if (contBlob > 2) {
					str = std::string("STOP");
					cv::putText(frame, str, cv::Point(20, 25), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
					cv::putText(frame, str, cv::Point(20, 25), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
				}
				else {
					str = std::string("PROIBIDO");
					cv::putText(frame, str, cv::Point(20, 25), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
					cv::putText(frame, str, cv::Point(20, 25), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
				}
			}
		}

		cv::imshow("VC - VIDEO", frame);
		/* Sai da aplica��o, se o utilizador premir a tecla 'q' */
		key = cv::waitKey(1);
		//Liberta��o de Mem�ria
		vc_image_free(imageBlob);
		vc_image_free(image2);

	}

	//Liberta��o de mem�ria
	vc_image_free(image);
	vc_image_free(imageHsv);
	vc_image_free(hsvSegAzul);
	vc_image_free(hsvSegVermelho);


	/* Fecha a janela */
	cv::destroyWindow("VC - VIDEO");

	/* Fecha o ficheiro de v�deo */
	capture.release();

	return 0;
}