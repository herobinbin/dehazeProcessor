#include "../include/nonLocalDehazeProcessor.h"
#include "../include/Transmission.h"
#include "../include/Atmosphere.h"
#include "../include/Darkchannel.h"
#include "../include/guidedfilter.h"
#include "../include/strutil.h"
#include <assert.h>

using namespace cv;
using namespace std;

void nonLocalDehazeProcessor::init(std::string sph_file)
{
    this->SPH_NUM = 2500;
	static float * gl_table = new float[SPH_NUM * 2];

	std::fstream f(sph_file, std::fstream::in);
    if( f.fail() ) return;
    char cmd[1024];
    int count = 0;
    while ( f.getline( cmd, 1024) )
    {
        std::string line( cmd );
        line = strutil::trim( line );
        strutil::Tokenizer stokenizer( line, " \t\r\n" );
        std::string token;
        
        for (int i = 0; i < 2; ++i) {
            stokenizer.nextToken();
            token = stokenizer.getToken();
            gl_table[count++] = strutil::parseString<float>(token);
        }

    }
    cvflann::Matrix<float> dataset(gl_table, SPH_NUM, 2);

    this->kdtree = new cvflann::Index< cvflann::L2_Simple<float> >( 
        dataset, cvflann::KDTreeSingleIndexParams(10,true) );
    (this->kdtree)->buildIndex();
}

void nonLocalDehazeProcessor::process()
{
	assert(this->src().data != NULL);
	if (this->src().type() != CV_8UC3)
		this->src().convertTo(this->src(), CV_8UC3, 255, 0);

	getDarkChannel(this->src(), this->darkchannel, 15);
	cv::Mat gray, temp;
	cv::cvtColor(this->src(), gray, cv::COLOR_BGR2GRAY);
	darkchannel = guidedFilter(gray, this->darkchannel, 15, 0.001);

	// cout << "darkchannel" << endl;
	// imshow("dark", this->darkchannel);
	getAtmosphere(this->darkchannel, this->src(), this->atmosphere);
	// cout << "atmosphere" << endl;
	getTransmission(this->src(), temp, this->atmosphere, *(this->kdtree), this->SPH_NUM);
	cv::medianBlur(temp, transmission, 5);
    transmission = guidedFilter(gray, transmission, 15, 0.001);// 
	// cout << "Transmission" << endl;
	imshow("t", this->transmission);
}



void nonLocalDehazeProcessor::hazeFree()
{
	this->src().convertTo(this->src(), CV_32FC3, 1.0 / 255, 0);
	// this->atmosphere.convertTo(this->atmosphere, CV_32FC3, 1.0 / 255, 0);   //vec3f
	this->transmission.convertTo(this->transmission, CV_32FC1, 1.0 / 255, 0);

	this->dst().create(this->src().size(), CV_8UC3);

    for(int i = 0; i != this->src().rows; ++i)
    {
        for(int j = 0; j != this->src().cols; ++j)
        {
            int idx = i*this->src().cols + j;;
            float r = this->transmission.at<float>(i, j) < 0.1 ? 0.1 : this->transmission.at<float>(i, j), 
                  t0 = (this->src().at<cv::Vec3f>(i, j)[0] - (1 - this->transmission.at<float>(i, j)) * this->atmosphere[0]) / r,
                  t1 = (this->src().at<cv::Vec3f>(i, j)[1] - (1 - this->transmission.at<float>(i, j)) * this->atmosphere[1]) / r,
                  t2 = (this->src().at<cv::Vec3f>(i, j)[2] - (1 - this->transmission.at<float>(i, j)) * this->atmosphere[2]) / r;
            
            this->dst().at<cv::Vec3b>(i, j)[0] = t0 < 0 ? : t0 > 1 ? 255 : t0*255;
            this->dst().at<cv::Vec3b>(i, j)[1] = t1 < 0 ? : t1 > 1 ? 255 : t1*255;
            this->dst().at<cv::Vec3b>(i, j)[2] = t2 < 0 ? : t2 > 1 ? 255 : t2*255;
        }
    }
}

void deHazeByNonLocalMethod(cv::Mat & input, cv::Mat & output, std::string sph_file)
{
	clock_t start = clock();
	nonLocalDehazeProcessor dp(sph_file);
	dp.setInput(input);
	dp.process();
	dp.hazeFree();
	dp.getOutput(output);
	cout << (clock() - start) / 1000000.0 << " s\n";
}

void deHazeByNonLocalMethod(nonLocalDehazeProcessor & dp, cv::Mat & input, cv::Mat & output)
{
    clock_t start = clock();
    dp.setInput(input);
    dp.process();
    dp.hazeFree();
    dp.getOutput(output);
    cout << (clock() - start) / 1000000.0 << " s\n";
}