#pragma once 
#include <vector>

namespace core {
    class Image{
        private:
            int rows;
            int cols;
            int maxIntensity;
            int bitsPerPixel; 
            double dpi;
            std::vector<std::vector<int>> data;

        public:
            Image() = default;
            Image(int rows, int cols, int maxIntensity = 255, int bitsPerPixel = 8, double dpi = 72.0);
            int& pixel(int x, int y);
            
            void fill(int value);
            void clampToRange();
            int getRows(){
                return rows;
            }

            int getCols(){
                return cols;
            }

            std::vector<std::vector<int>>& getData(){
                return data;
            }

            int getMaxIntensity(){ return maxIntensity; }
            int getBitsPerPixel(){ 
                return bitsPerPixel; 
            }
            void setBitsPerPixel(int k) { 
                bitsPerPixel = k; 
            }

            double getDpi(){ 
                return dpi; 
            }
            void setDpi(double newDpi) { 
                dpi = newDpi; 
            }
    };
}