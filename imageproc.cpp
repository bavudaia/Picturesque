using namespace std;
#include <iostream>
#include <fstream>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <algorithm>	
#include "omp.h"
#define HEADER_SIZE 54
#define THRESHOLD 256
#define NUM_THREADS 10
#define PI 3.141592653589793238462643383


/* Image library whose constructor loads the image into height*width 2D matrix and manipulates it 
 * using 5 algorithms
 * 1 . Smooth Filtering using a 3*3 gaussian filter  -- Test with Pepper.bmp(512*512) , man.bmp(1024*1024)
 * 2 . Noise Reduction by Median Filtering (5*5 window) --  Test with img.bmp, baboon_noise.bmp
 * 3 . Noise Reduction by Mean Filtering (5*5 Window)  -- Test with img.bmp, baboon_noise.bmp
 * 4 . Contrast improvement by using Historgram Equalization Algo -- Test with elaine.bmp man.bmp 
 * 5 . Image Swirl by using projection  -- Test with man.bmp
 * 
 * */
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
	/* Smooth Filter*/
	void smoothen();
	void smoothenParallel();
	/* Noise reduction by Median Filtering */
	void medianFilter();
	void medianFilterParallel();
	
	void filterTotal();
	int getHeight();
	int getWidth();
	double getGain();
	bool checkSol();
	
	/* Mean Filter for Noise Reduction*/
	void meanFilter();
	void meanFilterParallel();
	
	/* Improve Contrast of Image using Histogram Equalizer*/
	void histogramEqualizer();
	void histogramEqualizerParallel();
	void histogramEqualizerParallelNoLock();
	
	/* Image Swirl */
	void swirl();
	void swirlParallel();
	
	/*Test functions*/
	void smoothenTest(int num_threads);
	void swirlTest(int num_threads);
	void medianFilterTest(int num_threads);
	void meanFilterTest(int num_threads);
	void histogramEqualizerTest(int num_threads);
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
	imageOutputStream = new ofstream;
	imageInputStream->open(fileName, ios::in | ios::binary);
	if(imageInputStream)
	{
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
}

Image::~Image()
{
    delete imageInputStream;
    delete imageOutputStream;
    for(int i=0; i<height; i++){
        delete[] imageData[i];
        delete[] processedData[i];
        delete[] processedDataParallel[i];
    }

    delete[] imageData;
    delete[] processedData;
    delete[] processedDataParallel;
    delete[] imageHeaderRest;
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

/* All Sequential Algorithms Start -------------------------------------------*/


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

void Image::medianFilter()
{
	double start = omp_get_wtime();
	for(int i=2;i<height-2;i++)
	{
		for(int j=2;j<width-2;j++)
		{
			unsigned char* window = new unsigned char[25];
			int k=0;			
			/* get the 5*5 window */
			for(int a=i-2;a<=i+2;a++)
			{
				for(int b=j-2;b<=j+2;b++)
				{
					window[k++] = imageData[a][b];
				}
			}
			/* calc median for the window */
			std::sort(window,window+25);
			unsigned char median = window[12];
			/* Assign median val */
			processedData[i][j] = median;
		}
	}
	
	double end = omp_get_wtime();
	timeTakenSerial = end - start;
	cout<<"Median Filter time taken End : " <<timeTakenSerial<<"\n"; 
}

void Image::meanFilter()
{
	double start = omp_get_wtime();
	
	for(int i=2;i<height-2;i++)
	{
		for(int j=2;j<width-2;j++)
		{
			unsigned char* window = new unsigned char[25];
			int sum =0;
			
			/* get the 5*5 window */
			for(int a=i-2;a<=i+2;a++)
			{
				for(int b=j-2;b<=j+2;b++)
				{
					sum+= imageData[a][b];
				}
			}
			
			/* calc mean for the window */
			
			unsigned char mean = sum/25;
			/* Assign mean val */
			processedData[i][j] = mean;
			
		}
	}
	
	double end = omp_get_wtime();
	timeTakenSerial = end - start;
	cout<<"Mean Filter time taken End : " <<timeTakenSerial<<"\n"; 
}


/* Histogram Equalizer to improve image contrast */
void Image::histogramEqualizer()
{
	double start = omp_get_wtime();
	int hist[256];
	for(int i=0;i<256;i++)
	  hist[i] = 0;
	/* populate histogram with frequency*/
	for(int i=0;i<height;i++)
	{
		for(int j=0;j<width;j++)
		{
			unsigned char val = imageData[i][j];
			++hist[val];
		}
	}
	int localSum = 0, cumulativeMin = 255,firstFlag = false;
	
	/* prepare cumulative histogram */
	for(int i=0;i<256;i++)
	{
		if(hist[i])
		{
			if(!firstFlag)
			{
				firstFlag = true;
				cumulativeMin = hist[i];
			}
			hist[i] = localSum + hist[i];
			localSum = hist[i];
		}
	}	
	unsigned char normalized[256];
	for(int i=0;i<256;i++)
	  normalized[i] = 0;
	for(int i=0;i<256;i++)
	{
		if(hist[i])
		{
			double val = ((double)(hist[i] - cumulativeMin)) / ((double)(height*width - 1)) * 255.0;
			normalized[i] = std::round(val);
		}
	}
	
	for(int i=0;i<height;i++)
	{
		for(int j=0;j<width;j++)
		{
			unsigned char val = normalized[imageData[i][j]];
			processedData[i][j] = val;
		}
	}
	
	double end = omp_get_wtime();
	timeTakenSerial = end - start;
	cout<<"Histogram Equalizer Sequential time taken End : " <<timeTakenSerial <<"\n"; 
}

void Image::swirl()
{	
	double start = omp_get_wtime();
	double i0 = (height-1)/2.0;
	double j0 = (width -1)/2.0;
	for(int i=0;i<height;i++)
	{
		int ri=-1,rj=-1;
		for(int j=0;j<width;j++)
		{
			double di = i-i0;
			double dj = j-j0;
			double r = sqrt(di*di + dj*dj);
		    double theta = PI/256.0*r;
		    int ri = (int) (di*cos(theta) - dj*sin(theta) + i0);
		    int rj = (int) (di*sin(theta) + dj*cos(theta) + j0);
			if(ri >=0 && rj>=0 && ri<height&& rj<width)
				processedData[i][j] = imageData[ri][rj];
		}
	}
	double end = omp_get_wtime();
	timeTakenSerial = end - start;
	cout<<"Swirl Sequential time taken : "<<timeTakenSerial<<"\n";
}

/* All Sequential Algorithms End -------------------------------------------*/


/* All Parallel Algorithms and Test Start ---------------------------------------*/

void Image::smoothenParallel()
{
	//Measure metrics here 
	double start = omp_get_wtime();
	omp_set_num_threads(NUM_THREADS);
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
				  if(abs(avg - imageData[i][j]) <= THRESHOLD)
					processedDataParallel[i][j] = avg;
			}
		}
	}
	double end = omp_get_wtime();
	timeTakenParallel = end - start;
	cout<<"Smoothen time taken Parallel : " <<timeTakenParallel <<"\n"; 
	gain = timeTakenSerial/timeTakenParallel;
}

void Image::smoothenTest(int num_threads)
{
		//Measure metrics here 
	double start = omp_get_wtime();
	omp_set_num_threads(num_threads);
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
				  if(abs(avg - imageData[i][j]) <= THRESHOLD)
					processedDataParallel[i][j] = avg;
			}
		}
	}
	double end = omp_get_wtime();
	timeTakenParallel = end - start;
	gain = timeTakenSerial/timeTakenParallel;
	cout<<num_threads<<","<<gain<<"\n";
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
			//if(imageData[i][j] != processedDataParallel[i][j]) //to check if data is modified at all
			if(processedData[i][j] != processedDataParallel[i][j])
			  return false;
		}
	}
	return true;
}

/* Median Filter with window size 3*3 matrix for simplicity */
/* This is used for Noise reduction in images */
void Image::medianFilterParallel()
{
	double start = omp_get_wtime();
	omp_set_num_threads(NUM_THREADS);
	#pragma omp parallel for
	for(int i=2;i<height-2;i++)
	{
		for(int j=2;j<width-2;j++)
		{
			unsigned char* window = new unsigned char[25];
			int k=0;
			/* get the 5*5 window */
			for(int a=i-2;a<=i+2;a++)
			{
				for(int b=j-2;b<=j+2;b++)
				{
					window[k++] = imageData[a][b];
				}
			}
			/* calc median for the window */
			std::sort(window,window+25);
			unsigned char median = window[12];
			/* Assign median val */
			processedDataParallel[i][j] = median;
			
		}
	}
	
	double end = omp_get_wtime();
	timeTakenParallel = end - start;
	cout<<"Median Filter time taken End : " <<timeTakenParallel <<"\n"; 
	gain = timeTakenSerial/timeTakenParallel;
}

void Image::medianFilterTest(int num_threads)
{
	double start = omp_get_wtime();
	omp_set_num_threads(num_threads);
	#pragma omp parallel for
	for(int i=2;i<height-2;i++)
	{
		for(int j=2;j<width-2;j++)
		{
			unsigned char* window = new unsigned char[25];
			int k=0;
			/* get the 5*5 window */
			for(int a=i-2;a<=i+2;a++)
			{
				for(int b=j-2;b<=j+2;b++)
				{
					window[k++] = imageData[a][b];
				}
			}
			/* calc median for the window */
			std::sort(window,window+25);
			unsigned char median = window[12];
			/* Assign median val */
			processedDataParallel[i][j] = median;
			
		}
	}
	double end = omp_get_wtime();
	timeTakenParallel = end - start;
	gain = timeTakenSerial/timeTakenParallel;
	cout<<num_threads<<","<<gain<<"\n";
}

/* Mean Filter with window size 5*5 matrix for simplicity */
/* This is used for Noise reduction in images */
void Image::meanFilterParallel()
{
	double start = omp_get_wtime();
	omp_set_num_threads(NUM_THREADS);
	#pragma omp parallel for
	for(int i=2;i<height-2;i++)
	{
		for(int j=2;j<width-2;j++)
		{
			int sum =0;
			int k=0;
			/* get the 5*5 window */
			for(int a=i-2;a<=i+2;a++)
			{
				for(int b=j-2;b<=j+2;b++)
				{
					sum+= imageData[a][b];
				}
			}
			/* calc mean for the window */
			unsigned char mean = sum/25;
			/* Assign mean val */
			processedDataParallel[i][j] = mean;		
		}
	}
	double end = omp_get_wtime();
	timeTakenParallel = end - start;
	cout<<"Mean Filter time taken End : " <<timeTakenParallel <<"\n"; 
	gain = timeTakenSerial/timeTakenParallel;
}

void Image::meanFilterTest(int num_threads)
{
	double start = omp_get_wtime();
	omp_set_num_threads(num_threads);
	#pragma omp parallel for
	for(int i=2;i<height-2;i++)
	{
		for(int j=2;j<width-2;j++)
		{
			int sum =0;
			int k=0;
			/* get the 5*5 window */
			for(int a=i-2;a<=i+2;a++)
			{
				for(int b=j-2;b<=j+2;b++)
				{
					sum+= imageData[a][b];
				}
			}
			/* calc mean for the window */
			unsigned char mean = sum/25;
			/* Assign mean val */
			processedDataParallel[i][j] = mean;		
		}
	}
	double end = omp_get_wtime();
	timeTakenParallel = end - start;
	gain = timeTakenSerial/timeTakenParallel;
	cout<<num_threads<<","<<gain<<"\n";	
}

/* Parallel algo with locks for each index of hist[] */
void Image::histogramEqualizerParallel()
{
	/* Padding to avoid false sharing*/
	int PAD = 64;
	double start = omp_get_wtime();
	omp_set_num_threads(NUM_THREADS);
	int hist[256*PAD];
	for(int i=0;i<256*PAD;i+=PAD)
	  hist[i] = 0;
	
	
	omp_lock_t* lock = new omp_lock_t[256*PAD];
	for(int i=0;i<256*PAD;i+=PAD)
		omp_init_lock(lock+i);
		
	/* populate histogram with frequency*/
	#pragma omp parallel for
	for(int i=0;i<height;i++)
	{
		for(int j=0;j<width;j++)
		{
			int val = imageData[i][j]*PAD;
			omp_set_lock(lock + val);
			//#pragma omp critical
			++hist[val];
			omp_unset_lock(lock + val);
		}
	}
	for(int i=0;i<256*PAD;i+=PAD)
		omp_destroy_lock(lock+i);
	int localSum = 0, cumulativeMin = 255,firstFlag = false;
	
	/* prepare cumulative histogram */
	for(int i=0;i<256*PAD;i+=PAD)
	{
		if(hist[i])
		{
			if(!firstFlag)
			{
				firstFlag = true;
				cumulativeMin = hist[i];
			}
			hist[i] = localSum + hist[i];
			localSum = hist[i];
		}
	}	
	unsigned char normalized[256*PAD];
	for(int i=0;i<256*PAD;i+=PAD)
	{
		if(hist[i])
		{
			double val = ((double)(hist[i] - cumulativeMin)) / ((double)(height*width - 1)) * 255.0;
			normalized[i] = std::round(val);
		}
	}
	omp_set_num_threads(NUM_THREADS);
	#pragma omp parallel for
	for(int i=0;i<height;i++)
	{
		for(int j=0;j<width;j++)
		{
			processedDataParallel[i][j] = normalized[imageData[i][j]*PAD];
		}
	}
	
	double end = omp_get_wtime();
	timeTakenParallel = end - start;
	cout<<"Histogram Equalizer parallel time taken End : " <<timeTakenParallel <<"\n"; 
	gain = timeTakenSerial/timeTakenParallel;
}


void Image::histogramEqualizerParallelNoLock()
{
	int PAD = 1;
	double start = omp_get_wtime();
	omp_set_num_threads(NUM_THREADS);
	int hist[256*PAD];
	for(int i=0;i<256*PAD;i+=PAD)
	  hist[i] = 0;
	
	//#pragma omp parallel for
	for(int i=0;i<height;i++)
	{
		for(int j=0;j<width;j++)
		{
			int val = imageData[i][j]*PAD;
			++hist[val];
		}
	}
	int localSum = 0, cumulativeMin = 255,firstFlag = false;
	
	/* prepare cumulative histogram */
	for(int i=0;i<256*PAD;i+=PAD)
	{
		if(hist[i])
		{
			if(!firstFlag)
			{
				firstFlag = true;
				cumulativeMin = hist[i];
			}
			hist[i] = localSum + hist[i];
			localSum = hist[i];
		}
	}	
	unsigned char normalized[256*PAD];
	for(int i=0;i<256*PAD;i+=PAD)
	{
		if(hist[i])
		{
			double val = ((double)(hist[i] - cumulativeMin)) / ((double)(height*width - 1)) * 255.0;
			normalized[i] = std::round(val);
		}
	}
	omp_set_num_threads(NUM_THREADS);
	#pragma omp parallel for
	for(int i=0;i<height;i++)
	{
		for(int j=0;j<width;j++)
		{
			processedDataParallel[i][j] = normalized[imageData[i][j]*PAD];
		}
	}
	
	double end = omp_get_wtime();
	timeTakenParallel = end - start;
	cout<<"Histogram Equalizer parallel time taken End : " <<timeTakenParallel <<"\n"; 
	gain = timeTakenSerial/timeTakenParallel;
}
void Image::histogramEqualizerTest(int num_threads)
{
	int PAD = 1;
	double start = omp_get_wtime();
	omp_set_num_threads(num_threads);
	int hist[256*PAD];
	for(int i=0;i<256*PAD;i+=PAD)
	  hist[i] = 0;
	
	//omp_lock_t* lock = new omp_lock_t[256*PAD];
	//for(int i=0;i<256*PAD;i+=PAD)
		//omp_init_lock(lock+i);
	/* populate histogram with frequency*/	
	//#pragma omp parallel for  
	for(int i=0;i<height;i++)
	{
		for(int j=0;j<width;j++)
		{
			int val = imageData[i][j]*PAD;
			//omp_set_lock(lock + val);
			//#pragma omp critical   // degrades performance drastically
			++hist[val];
			//omp_unset_lock(lock + val);
		}
	}
	//for(int i=0;i<256*PAD;i+=PAD)
		//omp_destroy_lock(lock+i);
	int localSum = 0, cumulativeMin = 255,firstFlag = false;
	
	/* prepare cumulative histogram */
	for(int i=0;i<256*PAD;i+=PAD)
	{
		if(hist[i])
		{
			if(!firstFlag)
			{
				firstFlag = true;
				cumulativeMin = hist[i];
			}
			hist[i] = localSum + hist[i];
			localSum = hist[i];
		}
	}
	unsigned char normalized[256*PAD];
	for(int i=0;i<256*PAD;i+=PAD)
	{
		if(hist[i])
		{
			double val = ((double)(hist[i] - cumulativeMin)) / ((double)(height*width - 1)) * 255.0;
			normalized[i] = std::round(val);
		}
	}
	omp_set_num_threads(num_threads);
	#pragma omp parallel for
	for(int i=0;i<height;i++)
	{
		for(int j=0;j<width;j++)
		{
			processedDataParallel[i][j] = normalized[imageData[i][j]*PAD];
		}
	}
	
	double end = omp_get_wtime();
	timeTakenParallel = end - start;
	gain = timeTakenSerial/timeTakenParallel;	
	cout<<num_threads<<","<<gain<<"\n";
}



void Image::swirlParallel()
{	
	double i0 = (height-1)/2.0;
	double j0 = (width -1)/2.0;
	omp_set_num_threads(NUM_THREADS);
	double start = omp_get_wtime();
	#pragma omp parallel for
	for(int i=0;i<height;i++)
	{
		int ri=-1,rj=-1;
		for(int j=0;j<width;j++)
		{
			double di = i-i0;
			double dj = j-j0;
			double r = sqrt(di*di + dj*dj);
		    double theta = PI/256.0*r;
		    int ri = (int) (di*cos(theta) - dj*sin(theta) + i0);
		    int rj = (int) (di*sin(theta) + dj*cos(theta) + j0);
			if(ri >=0 && rj>=0 && ri<height&& rj<width)
				processedDataParallel[i][j] = imageData[ri][rj];
		}
	}	
	double end = omp_get_wtime();
	timeTakenParallel = end - start;
	cout<<"Swirl Parallel time taken : " <<timeTakenParallel<<"\n";
	double gain = timeTakenSerial/timeTakenParallel;
}

void Image::swirlTest(int num_threads)
{
	double i0 = (height-1)/2.0;
	double j0 = (width -1)/2.0;
	omp_set_num_threads(num_threads);
	double start = omp_get_wtime();
	#pragma omp parallel for
	for(int i=0;i<height;i++)
	{
		int ri=-1,rj=-1;
		for(int j=0;j<width;j++)
		{
			double di = i-i0;
			double dj = j-j0;
			double r = sqrt(di*di + dj*dj);
		    double theta = PI/256.0*r;
		    int ri = (int) (di*cos(theta) - dj*sin(theta) + i0);
		    int rj = (int) (di*sin(theta) + dj*cos(theta) + j0);
			if(ri >=0 && rj>=0 && ri<height&& rj<width)
				processedDataParallel[i][j] = imageData[ri][rj];
		}
	}	
	double end = omp_get_wtime();
	timeTakenParallel = end-start;
	gain = timeTakenSerial/timeTakenParallel;
	cout<<num_threads<<","<<gain<<"\n";
}


/* All Parallel Algorithms and Tests End ---------------------------------------*/

int main()
{
    char imageFileName[30] ;
    cout << "Enter Image File Name : " ;
    fgets(imageFileName,15,stdin);
    strtok(imageFileName,"\n");
    
	char input[30] = "image/";
	strcat(input,imageFileName);
	
	Image i(input);
	i.readImage();
	cout<<"What type of Image Processing \n";
	cout<<"1. Smoothening\n";
	cout<<"2. Noise Reduction by Median Filtering\n";
	cout<<"3. Noise Reduction by Mean Filtering \n";
	cout<<"4. Improving contrast by Histogram Equalization \n";
	cout<<"5. Swirl the Image\n";
	cout<<"6. Performance Report\n";
	cout<<"Enter Choice : ";
	int choice=0;
	cin>>choice;
	bool res = false;
	if(choice == 1)
	{
		i.smoothen();
		i.writeImage("image/processed.bmp",false);
		i.smoothenParallel();
		i.writeImage("image/processedParallel.bmp",true);
		cout<< "Gain compared to Sequencial Algo is " <<  i.getGain() << " times\n";
		res = i.checkSol();
	}
	else if(choice == 2)
	{
		i.medianFilter();
		i.writeImage("image/processed.bmp",false);
		i.medianFilterParallel();
		i.writeImage("image/processedParallel.bmp",true);
		cout<< "Gain compared to Sequencial Algo is " <<  i.getGain() << " times\n";
		res = i.checkSol();
	}
	else if(choice == 3)
	{
		i.meanFilter();
		i.writeImage("image/processed.bmp",false);
		i.meanFilterParallel();
		i.writeImage("image/processedParallel.bmp",true);
		cout<< "Gain compared to Sequencial Algo is " <<  i.getGain() << " times\n";
		res = i.checkSol();
	}
	else if(choice == 4)
	{
		i.histogramEqualizer();
		i.writeImage("image/processed.bmp",false);
		i.histogramEqualizerParallelNoLock();
		i.writeImage("image/processedParallel.bmp",true);
		cout<< "Gain compared to Sequencial Algo is " <<  i.getGain() << " times\n";
		res = i.checkSol();
	}
    else if(choice == 5)
	{
		i.swirl();
		i.writeImage("image/processed.bmp",false);
		i.swirlParallel();
		i.writeImage("image/processedParallel.bmp",true);
		cout<< "Gain compared to Sequencial Algo is " <<  i.getGain() << " times\n";
		res = i.checkSol();
	}
	else if(choice == 6)
	{
		int iters = 50;
		
		cout<<"Performance Report -- Number of threads vs Gain \n";
		cout<<"Smooth Filtering\n";
		i.smoothen();
		
		for(int x=1;x<=iters;x++)
			i.smoothenTest(x);
		cout<<"Median Filter\n";
		i.medianFilter();
		for(int x=1;x<=iters;x++)
			i.medianFilterTest(x);
		cout<<"Mean Filter\n";
		i.meanFilter();
		for(int x=1;x<=iters;x++)
			i.meanFilterTest(x);
			
		cout<<"Histogram Equalization\n";
		i.histogramEqualizer();
		for(int x=1;x<=iters;x++)
			i.histogramEqualizerTest(x);
			
		cout<<"Swirl\n";
		i.swirl();
		for(int x=1;x<=iters;x++)
			i.swirlTest(x);
			
	}
	if(res)
	  cout<< "Results Match" <<"\n";
	else
	  cout<<" Results Do not match" << "\n";
	return 0;
}

