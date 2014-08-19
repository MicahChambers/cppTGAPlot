/******************************************************************************
 * Copyright 2014 Micah C Chambers (micahc.vt@gmail.com)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * 	http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @file tga_test1.cpp Tests function plotting, 
 *
 *****************************************************************************/

#include <iostream>
#include <cstdlib>

#include "tgaImage.h"
	
TGAImage::TGAImage(size_t xres, size_t yres)
{
	clear();
	res[0] = xres;
	res[1] = yres;
}

void TGAImage::clear()
{
	res[0] = 1024;
	res[1] = 768;
	xrange[0] = NAN;
	xrange[1] = NAN;
	yrange[0] = NAN;
	yrange[1] = NAN;
	axes = false;
	
	colors.clear();
	colors.push_back(StyleT("r"));
	colors.push_back(StyleT("g"));
	colors.push_back(StyleT("b"));
	colors.push_back(StyleT("y"));
	colors.push_back(StyleT("c"));
	colors.push_back(StyleT("p"));
	colors.push_back(StyleT("-r"));
	colors.push_back(StyleT("-g"));
	colors.push_back(StyleT("-b"));
	colors.push_back(StyleT("-y"));
	colors.push_back(StyleT("-c"));
	colors.push_back(StyleT("-p"));

	curr_color = colors.begin();

	axes = false;

	funcs.clear();
	arrs.clear();
}

/**
 * @brief Writes the output image to the given file
 *
 * @param fname File name to write to.
 */
void TGAImage::write(std::string fname)
{
	write(res[0], res[1], fname);
}

/**
 * @brief If the ranged haven't been provided, then autoset them
 */
void TGAImage::computeRange(size_t xres)
{
	bool pad_x = false;;
	bool pad_y = false;;

	// compute range
	if(!isnormal(xrange[0])) {
		// compute minimum
		xrange[0] = INFINITY;
		for(auto& arr : arrs) {
			auto& xarr = std::get<1>(arr);
			for(auto& v: xarr) {
				if(v < xrange[0]) 
					xrange[0] = v;
			}
		}

		pad_lx = true;
	}
	if(!isnormal(xrange[1])) {
		// compute minimum
		xrange[1] = -INFINITY;
		for(auto& arr : arrs) {
			auto& xarr = std::get<1>(arr);
			for(auto& v: xarr) {
				if(v > xrange[1]) 
					xrange[1] = v;
			}
		}

		double pad = (xrange[1]-xrange[0])*1.05;
		if(pad_lx) 
			xrange[0] -= pad/2;
		xrange[1] += pad/2;
	}

	if(!isnormal(yrange[0])) {
		// compute minimum
		yrange[0] = INFINITY;

		// from arrays
		for(auto& arr : arrs) {
			auto& yarr = std::get<2>(arr);
			for(auto& v: arr) {
				if(v < yrange[0]) 
					yrange[0] = v;
			}
		}
		
		// from functions, use breaking up x range
		for(auto& func: funcs) {
			double step = (xrange[1]-xrange[0])/xres;
			for(int64_t ii=0; ii<xres; ii++) {
				double x = xrange[0]+ii*step;
				double y = func(x);
				if(y < yrange[0])
					yrange[0] = y;
			}
		}
		pad_ly = true;
	}
	
	if(!isnormal(yrange[1])) {
		// compute minimum
		yrange[1] = -INFINITY;

		// from arrays
		for(auto& arr : arrs) {
			auto& yarr = std::get<2>(arr);
			for(auto& v: yarr) {
				if(v > yrange[1]) 
					yrange[1] = v;
			}
		}
		
		// from functions, use breaking up x range
		for(auto& func: funcs) {
			double step = (xrange[1]-xrange[0])/xres;
			for(int64_t ii=0; ii<xres; ii++) {
				double x = xrange[0]+ii*step;
				double y = func(x);
				if(y > yrange[1])
					yrange[1] = y;
			}
		}

		double pad = (yrange[1]-yrange[0])*1.05;
		if(pad_ly) 
			yrange[0] -= pad/2;
		yrange[1] += pad/2;
	}
}

/**
 * @brief Write the output image with the given (temporary) resolution.
 * Does not affect the internal resolution
 *
 * @param xres X resolution
 * @param yres Y resolution
 * @param fname Filename
 */
void TGAImage::write(size_t xres, size_t yres, std::string fname)
{
	std::ofstream o(fname.c_str(), std::ios::out | std::ios::binary);

	//Write the header
	o.put(0); //ID
	o.put(0); //Color Map Type
	o.put(10); // run length encoded truecolor
	
	// color map
	o.put(0);
	o.put(0);
	o.put(0);
	o.put(0);
	o.put(0);
	
	//X origin
	o.put(0);
	o.put(0);

	//Y origin
	o.put(0);
	o.put(0);

	//width
	o.put((xres & 0x00FF));
	o.put((xres & 0xFF00) / 256);
	
	//height
	o.put((yres & 0x00FF));
	o.put((yres & 0xFF00) / 256);
	
	//depth
	o.put(32); /* 8 bit bitmap */
	
	//descriptor
	o.put(8); // 8 for RGBA

	// before performing run-length encoding, we need to fill a buffer 
	rgba* buffer = new rgba[xres*yres];

	//////////////////////////////////////////////////////////////////////////
	// fill buffer
	//////////////////////////////////////////////////////////////////////////
	computeRange(xres);

	double weights[2][2];
	int64_t neighbors[2][2];
	double xstep = (xrange[1]-xrange[0])/xres;
	double ystep = (yrange[1]-yrange[0])/yres;
	// start with buffers, interpolating between points
	for(auto& arr: arrs) {
		auto& sty = std::get<0>(arr);
		auto& xarr = std::get<1>(arr);
		auto& yarr = std::get<2>(arr);
		assert(xarr.size() == yarr.size());

		for(size_t ii=1; ii<xarr.size(); ii++) {
			double xp = (xarr[ii-1]-xrange[0])/xstep;
			double xf = (xarr[ii]-xrange[0])/xstep;
			double dx = xf-xp;
			double yp = (yarr[ii-1]-yrange[0])/ystep;
			double yf = (yarr[ii]-yrange[0])/ystep;
			double dy = yf-yp;
			
			// we want to take steps less than 1 in the fastest moving direction
			if(dx > dy) {
				dy /= (dx+1);
				dx /= (dx+1);
			} else {
				dx /= (dy+1);
				dy /= (dy+1);
			}

			for( ; xp <= xf && yp <= yf; xp+=dx, yp+=dy) {
				int64_t xi = std::max(std::min(xres-1, round(xp)), 0);
				int64_t yi = std::max(std::min(yres-1, round(yp)), 0);
				buffer[yi*xres+xi][0] = sty.rgba[0];
				buffer[yi*xres+xi][1] = sty.rgba[1];
				buffer[yi*xres+xi][2] = sty.rgba[2];
				buffer[yi*xres+xi][3] = sty.rgba[3];
			}
		}
	}
	
	for(auto& func: funcs) {
		auto& sty = std::get<0>(func);
		auto& foo = std::get<1>(func);

		double yip = NAN; // previous y index
		double yi; // y index
		double xx = xrange[0];
		while(xx < xrange[1]) {
			double xbase = xx;
			double dx = xstep;
			double yy;
			do {
				xx = xbase + dx;
				yy = foo(xx);
				yi = (yy-yrange[0])/ystep;
				dx /= 2;
			} while(!((yip - yi) < 1));
			yip = yi;
			int64_t yind = round(yi);
			int64_t xind = round((xx-xrange[0])/xstep);
			
			buffer[yind*xres + xind][0] = sty.rgba[0];
			buffer[yind*xres + xind][1] = sty.rgba[1];
			buffer[yind*xres + xind][2] = sty.rgba[2];
			buffer[yind*xres + xind][3] = sty.rgba[3];
		}
	}

	// draw axes
	if(axes) {
		// TODO
	}

	if(range != 0) {
		//Write the pixel data
		if(log) {
			for(uint32_t ii=0; ii < in.size(); ii++)
				o.put((unsigned char)(255*std::log(in[ii]-min+1)/range));
		} else {
			for(uint32_t ii=0; ii < in.size(); ii++)
				o.put((unsigned char)(255*(in[ii]-min)/range));
		}
	} else {
		for(uint32_t ii=0; ii < in.size(); ii++)
			o.put(0);
	}

	//close the file
	o.close();
}

/**
 * @brief Sets the x range. To use the extremal values from input arrays
 * just leave these at the default (NAN's)
 *
 * @param low Lower bound
 * @param high Upper bound
 */
void TGAImage::setXRange(double low, double high)
{
	xrange[0] = low;
	xrange[1] = high;
}

/**
 * @brief Sets the y range. To use the extremal values from input arrays
 * and computed yvalues from functions, just leave these at the default
 * (NAN's)
 *
 * @param low Lower bound
 * @param high Upper bound
 */
void TGAImage::setYRange(double low, double high)
{
	yrange[0] = low;
	yrange[1] = high;
}

/**
 * @brief Sets the default resolution
 *
 * @param xres Width of output image
 * @param yres Height of output image
 */
void TGAImage::setRes(size_t xres, size_t yres)
{
	res[0] = xres;
	res[1] = yres;
}

void TGAImage::addFunc(double(*f)(double))
{
	addfFunc(*curr_color, f);
	curr_color++;
	if(curr_color == colors.end())
		curr_color = colors.begin();
}

void TGAImage::addFunc(const std::string& style, double(*f)(double))
{
	StyleT tmps(style);
	addFunc(tmps, f);
}

void TGAImage::addFunc(const std::string& style, double(*f)(double))
{
	funcs.push_back(std::make_tuple<StyleT, (double)*(double)>(style, f));
}

void TGAImage::addArray(size_t sz, double* array)
{
	std::vector<double> tmpx(sz);
	std::vector<double> tmpy(sz);
	for(size_t ii=0; ii<sz; ii++) {
		tmpx[ii] = ii;
		tmpy[ii] = array[ii];
	}
	
	arrs.push_back(std::make_tuple<StyleT,vector<double>,vector<double>(
				*curr_color, tmpx, tmpy));

	if(curr_color == colors.end())
		curr_color = colors.begin();
}

void TGAImage::addArray(size_t sz, double* xarr, double* yarr)
{
	std::vector<double> tmpx(sz);
	std::vector<double> tmpy(sz);
	for(size_t ii=0; ii<sz; ii++) {
		tmpx[ii] = xarr[ii];
		tmpy[ii] = yarr[ii];
	}

	arrs.push_back(std::make_tuple<StyleT,vector<double>,vector<double>(
				*curr_color, tmpx, tmpy));
	
	if(curr_color == colors.end())
		curr_color = colors.begin();
};

void TGAImage::addArray(const std::string& style, size_t sz, double* array)
{
	std::vector<double> tmpx(sz);
	std::vector<double> tmpy(sz);
	for(size_t ii=0; ii<sz; ii++) {
		tmpx[ii] = xarr[ii];
		tmpy[ii] = yarr[ii];
	}
	tmpstyle(style);
	arrs.push_back(std::make_tuple<StyleT,vector<double>,vector<double>(style,
				tmpx, tmpy));
}

void TGAImage::addArray(const StyleT& style, size_t sz, double* xarr, double* yarr)
{
	std::vector<double> tmpx(sz);
	std::vector<double> tmpy(sz);
	for(size_t ii=0; ii<sz; ii++) {
		tmpx[ii] = xarr[ii];
		tmpy[ii] = yarr[ii];
	}

	arrs.push_back(std::make_tuple<StyleT,vector<double>,vector<double>(
				style, tmpx, tmpy));
}

};

