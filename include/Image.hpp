#pragma once 
#include <vector>

namespace core {
    class Image{
        private:
            int rows;
            int cols;
            int maxIntensity;
            std::vector<std::vector<int>> data;

        public:
            Image(int rows, int cols, int maxIntensity = 255);
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
    };
}