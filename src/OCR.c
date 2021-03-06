#include <stdio.h>
#include <err.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include "Matrix.h"
#include "Preprocessing/Preprocessing.h"
#include "Preprocessing/Filters.h"
#include "Segmentation/Segmentation.h"
#include "NeuralNetwork/NeuralNetwork.h"
#include "Postprocessing/Postprocessing.h"
#include "GUI/GUI.h"
#include "GUI/DebugGUI.h"



char* lastText;



char NNFindChar(unsigned char** matrix)
{
	// Generate NN inputs
	double input[576];
	for (int i = 0; i < 24; i++)
	{
		for (int j = 0; j < 24; j++)
		{
			input[i * 24 + j] = (double)matrix[i][j];
		}
	}

	// Predict output
	double *output = Predict(input);

	// Find char
	char res[85] = {
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
		'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
		'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
		'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
		'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
		'(', ')', '=', '+', ';', '.', '\'', '#', '&', '*', '-', '_',
		'{', '!', '}', ',', ':', '/', '?', '\\', '[', ']', '@'
	};
	int max = 0;
	for (int i = 0; i < 85; i++)
	{
		if (output[max] < output[i])
			max = i;
	}
	return res[max];
}

void OCR_Debug(char* path)
{
	Image image;
	image.path = path;
	image = LoadImageAsGrayscale(image);
	// image = Blur(image);
	// image = Sharpen(image);
	// image = GaussianBlur(image);
	image = BinarizeImage(image);

	Text text = Segmentation(image);

	StartDebugGUI(image, text);
}

char *OCR_Start(
	char *path,
	int enableDebugMode,
	int enableFilters,
	int enablePostprocessing
)
{
	// Start OCR
	Image image;
	image.path = path;
	image = LoadImageAsGrayscale(image);
	if(enableFilters)
	{
		image = Blur(image);
		image = Sharpen(image);
		image = Blur(image);
		image = GaussianBlur(image);
	}
	image = BinarizeImage(image);

	Start(576, 595, 85);
	Load("resources/nn.data");

	Text text = Segmentation(image);

	// Prepare outputText
	int numberOfCharacters = 0;
	for (int p = 0; p < text.numberOfParagraphs; p++)
	{
		for (int l = 0; l < text.paragraphs[p].numberOfLines; l++)
		{
			for (int w = 0; w < text.paragraphs[p].lines[l].numberOfWords; w++)
			{
				numberOfCharacters +=
					text.paragraphs[p].lines[l].words[w].numberOfCharacters;
				numberOfCharacters += 1;
			}
			numberOfCharacters += 1;
		}
		numberOfCharacters += 1;
	}
	char *outputText = malloc(sizeof(char) * numberOfCharacters * 2);

	// Explore text
	int i = 0;
	for (int p = 0; p < text.numberOfParagraphs; p++)
	{
		Paragraph paragraph = text.paragraphs[p];
		for (int l = 0; l < paragraph.numberOfLines; l++)
		{
			for (int w = 0; w < paragraph.lines[l].numberOfWords; w++)
			{
				Word word = paragraph.lines[l].words[w];
				char wordText[word.numberOfCharacters];
				for (int c = 0; c < word.numberOfCharacters; c++)
				{
					Character character = word.characters[c];

					char characterFound = NNFindChar(character.matrix);
					if (enablePostprocessing)
					{
						wordText[c] = characterFound;
					}
					else
					{
						if(characterFound == '&')
						{
							outputText[i] = '&';
							i++;
							outputText[i] = 'a';
							i++;
							outputText[i] = 'm';
							i++;
							outputText[i] = 'p';
							i++;
							outputText[i] = ';';
						}
						else
						{
							outputText[i] = characterFound;
						}
						
						i++;
					}
				}
				wordText[word.numberOfCharacters] = '\0';
				
				if (enablePostprocessing)
				{
					char *correctWordText;
					correctWordText = Spellcheck(wordText);
					// printf("Spellcheck: %s->%s\n", wordText, correctWordText);
					while(*correctWordText != '\0')
					{
						outputText[i] = *correctWordText;
						i++;
						correctWordText++;
					}
				}
				
				outputText[i] = ' ';
				i++;
			}
			outputText[i] = '\n';
			i++;
		}
		outputText[i] = '\n';
		outputText[i + 1] = '\n';
		i += 2;
	}
	outputText[i] = '\0';

	// Write text to GUI
	if (enableDebugMode)
	{
		StartDebugGUI(image, text);
	}
	lastText = outputText;

	// Free
	DestroyCharMatrix(image.grayscale, image.height);
	DestroyCharMatrix(image.binarized, image.height);
	for (int p = 0; p < text.numberOfParagraphs; p++)
	{
		for (int l = 0; l < text.paragraphs[p].numberOfLines; l++)
		{
			for (int w = 0; w < text.paragraphs[p].lines[l].numberOfWords; w++)
			{
				Word word = text.paragraphs[p].lines[l].words[w];
				int nbc = word.numberOfCharacters;
				for (int c = 0; c < nbc; c++)
				{
					Character character = word.characters[c];
					DestroyCharMatrix(character.matrix, 24);
				}
				free(text.paragraphs[p].lines[l].words[w].characters);
			}
			free(text.paragraphs[p].lines[l].words);
		}
		free(text.paragraphs[p].lines);
	}
	free(text.paragraphs);

	return outputText;
}

void OCR_ExportAsTextFile(char* path)
{
	FILE *f = fopen(path, "w");
	fputs(lastText, f);
	fclose(f);
}