using namespace std;
#include <iostream>
#include <fstream>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include "omp.h"
#define HEADER_SIZE 54
#define THRESHOLD 256
#define NUM_THREADS 5
class Image
{
	private:
	
    unsigned char** imageData;
	unsigned char** processedData;
    unsigned char** processedDataParallel;
	unsigned char imageHeaderFirst54[54];
	unsigned char* imageHeaderRest;
	ifstream* imageInputStream;
	ofstream* imageOutputStream;
	int height;
	int width;
	double timeTakenSerial;
	double timeTakenParallel;
	double gain;
	int filter[3][3] =  {{1,1,1},{1,2,1},{1,1,1}} ;
	//int filter[3][3] =  {{0,0,0},{0,2,0},{0,0,0}} ;
	int offsetToStart;
	int filterSum;
	public:
	
	Image(const char* fileName);
	~Image();
	void readImage();
	void writeImage(const char* fileName, bool isParallel);
	void printImageArray();
	void smoothen();
	void smoothenParallel();
	void filterTotal();
	int getHeight();
	int getWidth();
	double getGain();
	bool checkSol();
};

double Image::getGain()
{
	return gain;
}
int Image::getHeight()
{
	return height;
}


int Image::getWidth()
{
	return width;
}

	/* Initialize image Array and result image Arrays */
	/*start*/

Image::Image(const char* fileName)
{
	
	//filter = {{1,1,1},{1,2,1},{1,1,1}};
	filterTotal();
	imageInputStream = new ifstream;
	imageInputStream->open(fileName, ios::in | ios::binary);
	imageInputStream->seekg(0,ios::beg);
	imageInputStream->read(reinterpret_cast<char*> (imageHeaderFirst54) , HEADER_SIZE);
	/*
	for(int i=0;i<54;i++)
	{
		int x = *(int *)&imageHeaderFirst54[i];
		cout<< i << "  : " << x <<"\n";
	}
	*/
	 
	width = *(int *)&imageHeaderFirst54[18];
	height = *(int *)&imageHeaderFirst54[22];
	offsetToStart = *(int *)&imageHeaderFirst54[10];
	cout<<"Start of image" << offsetToStart << "\n";
	imageHeaderRest = new unsigned char[offsetToStart-HEADER_SIZE];
	
	imageInputStream->read(reinterpret_cast<char*> (imageHeaderRest)  , offsetToStart- HEADER_SIZE);
	
	imageData = new unsigned char* [height];
	processedData = new unsigned char* [height];
	processedDataParallel = new unsigned char* [height];
	
	for(int i=0;i<height;i++)
	{
		imageData[i] = new unsigned char[width];
		processedData[i] = new unsigned char[width];
		processedDataParallel[i] = new unsigned char[width];
	}

	imageInputStream->seekg(0,ios::end);
	int len = imageInputStream->tellg();
	cout<<"Length : " << len <<"\n";
	cout<< "Expected Width : "<<(len-offsetToStart)/height <<"\n";
	imageInputStream->seekg(offsetToStart,ios::beg);
}

Image::~Image()
{
    delete imageInputStream;
    
    for(int i=0; i<height; i++){
        delete[] imageData[i];
        delete[] processedData[i];
        delete[] processedDataParallel[i];
    }

    delete[] imageData;
    delete[] processedData;
    delete[] processedDataParallel;
    delete[] imageHeaderRest;
    //delete[] imageHeaderFirst54;
}

void Image::readImage()
{

	cout<<"width : " << width << "\n" << "height : " << height<<"\n";
	for(int i=0;i<height;i++)
	{

			imageInputStream->read(reinterpret_cast<char*> (imageData[i]),width);
		
		//cout<< "Image Data Length Row "<< i <<"  : " << strlen(reinterpret_cast<char *> (imageData[i])) <<"\n"; 
	  
		memcpy(reinterpret_cast<void*>(processedData[i]), reinterpret_cast<void*>(imageData[i]),width);
		memcpy(reinterpret_cast<void*>(processedDataParallel[i]), reinterpret_cast<void*>(imageData[i]),width);
	}
	/* Test if both arrays are same after copy */
	
	
	for(int i=0;i<height;i++)
	{
		for(int j=0;j<width;j++)
		{
			if(processedData[i][j] != imageData[i][j])
			{
				cout<< "Not matching at " << i << ", "  << j << "Processed " << (int)processedData[i][j] << "Orig image " << (int)imageData[i][j] << " \n";
				break;
			}
		}
	}
	imageInputStream->close();
}

void Image::writeImage(const char* fileName, bool isParallel)
{
	imageOutputStream = new ofstream;
	imageOutputStream->open(fileName , ios::out | ios::binary | ios::trunc);
	imageOutputStream->write(reinterpret_cast<char*> (imageHeaderFirst54) , HEADER_SIZE);
	imageOutputStream->write(reinterpret_cast<char*> (imageHeaderRest) , offsetToStart-HEADER_SIZE);
	for(int i=0;i<height;i++)
	{
		
		if(!isParallel)
		  imageOutputStream->write(reinterpret_cast<char*> (processedData[i]), width);
		else
		  imageOutputStream->write(reinterpret_cast<char*> (processedDataParallel[i]), width);
		
		//imageOutputStream->write(reinterpret_cast<char*> (imageData[i]), width);
	}
	imageOutputStream->close();
}

void Image::printImageArray()
{
	for(int i=0;i<height;i++)
	{
		for(int j=0;j<width;j++)
		{
			cout<<(int)imageData[i][j]<< " " ;
		}
		cout<< "\n";
	}
}

void Image::smoothen()
{
	//Measure metrics here 
	double start = omp_get_wtime();
	for(int i=1;i<height-1;i++)
	{
		for(int j=1;j<width-1;j++)
		{
			
			int sum = filter[0][0] * imageData[i-1][j-1] + 
                          filter[0][1] * imageData[i-1][j]   + 
                          filter[0][2] * imageData[i-1][j+1] + 
                          filter[1][0] * imageData[i][j-1]   + 
                          filter[1][1] * imageData[i][j]     + 
                          filter[1][2] * imageData[i][j+1]   +
                          filter[2][0] * imageData[i+1][j-1] + 
                          filter[2][1] * imageData[i+1][j]   + 
                          filter[2][2] * imageData[i+1][j+1];  
           
           int avg = sum/filterSum;
           if(abs(avg - imageData[i][j]) <= THRESHOLD)
              processedData[i][j] = avg;
		}
	}
	double end = omp_get_wtime();
	timeTakenSerial = end -start;
	cout<<"Smoothen time taken sequential : " <<timeTakenSerial <<"\n"; 
}

void Image::smoothenParallel()
{
	//Measure metrics here 
	double start = omp_get_wtime();
	omp_set_num_threads(5);
	#pragma omp parallel
	{
		#pragma omp for
		for(int i=1;i<height-1;i++)
		{
			for(int j=1;j<width-1;j++)
			{
				
				int sum = filter[0][0] * imageData[i-1][j-1] + 
							  filter[0][1] * imageData[i-1][j]   + 
							  filter[0][2] * imageData[i-1][j+1] + 
							  filter[1][0] * imageData[i][j-1]   + 
							  filter[1][1] * imageData[i][j]     + 
							  filter[1][2] * imageData[i][j+1]   +
							  filter[2][0] * imageData[i+1][j-1] + 
							  filter[2][1] * imageData[i+1][j]   + 
							  filter[2][2] * imageData[i+1][j+1];  
			   
				  int avg = sum/filterSum;
				  processedDataParallel[i][j] = avg;
			}
		}
	}
	double end = omp_get_wtime();
	timeTakenParallel = end - start;
	cout<<"Smoothen time taken Parallel : " <<timeTakenParallel <<"\n"; 
	gain = timeTakenSerial/timeTakenParallel;
}

void Image::filterTotal()
{
    filterSum = 0;
	for(int i=0;i<3;i++)
	  for(int j=0;j<3;j++)
	    filterSum+= filter[i][j];
	
}

bool Image::checkSol()
{
	for(int i=0;i<height;i++)
	{
		for(int j=0;j<width;j++)
		{
			//if(imageData[i][j] != processedDataParallel[i][j]) to check if data is modified at all
			if(processedData[i][j] != processedDataParallel[i][j])
			  return false;
		}
	}
	return true;
}


int main()
{
	Image i("image/george.bmp");
	i.readImage();
	//i.printImageArray();
	i.smoothen();
	i.writeImage("image/processed.bmp",false);
	i.smoothenParallel();
	i.writeImage("image/processedParallel.bmp",true);
	cout<< "Gain compared to Sequencial Algo is " <<  i.getGain() << " times\n";
	bool res = i.checkSol();
	if(res)
	  cout<< "Results Match" <<"\n";
	else
	  cout<<" Results Do not match" << "\n";
	return 0;
}

