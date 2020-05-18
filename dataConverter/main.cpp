//
//  main.cpp
//  dataConverter
//
//  Created by Juraj Antas on 18/05/2020.
//  Copyright © 2020 Sygic a.s. All rights reserved.
//

#include <iostream>
#include <string>
#include <limits>
//bez zbytocnych hovadin
//#include <simd/simd.h>

//nacitaj gps, acc, gyro, alti
//a vytlac csv kde bude:
//timestamp, acc, gyro, gps.speed, alti.pressure
//gps a alti su duplikovane do frequncie acc/gyra

using namespace std;

struct Double4 {
    double x{0};
    double y{0};
    double z{0};
    double timestamp{0};
    
    Double4& operator+=(const Double4& d) {
        x+=d.x;
        y+=d.y;
        z+=d.z;
        timestamp+=d.timestamp;
        return *this;
    }
    
    Double4 operator/(double m) {
        return Double4{x/m,y/m,z/m,timestamp/m,};
    }
};

struct Altimeter {
    double timestamp{0};
    double altitude{0};
    double pressure{0};
};

struct GpsPosition {
    double timestamp{0};
    double latitude{0};
    double longitude{0};
    double speed{0};
    double course{0};
    double horizontalAccuracy{0};
    double verticalAccuracy{0};
    double altitude{0};
};

//vrati pointer a pocet objektov
Double4* loadAccGyro(const char* filepath, long& count ) {
    FILE* file = fopen(filepath, "r");
    if (file == nullptr) {
        count = 0;
        return nullptr;
    }
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    count = size / sizeof(Double4);
    Double4* array = new Double4[count];
    fread(array, sizeof(Double4), count, file);
    return array; //mazat to nejdeme ;)
}

Double4* movingAverage(Double4* array, long count) {
    Double4* movingAverage = new Double4[count];
    
    if (count < 30) {
        return array;
    }
    
    memcpy(movingAverage, array, count * sizeof(Double4));
    long konstant = 30;
    for(long x=konstant; x< count;x++) {
        Double4 temp;
        for (long y = 0; y < konstant; y++) {
            temp += array[x-y];
        }
        movingAverage[x] = temp / konstant;
    }
    

    return movingAverage;
}

Altimeter* loadAltimeter(const char* filepath, long& count) {
    FILE* file = fopen(filepath, "r");
    if (file == nullptr) {
        count = 0;
        return nullptr;
    }
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    count = size / sizeof(Altimeter);
    Altimeter* array = new Altimeter[count];
    fread(array, sizeof(Altimeter), count, file);
    return array; //mazat to nejdeme ;)
}

GpsPosition* loadGps(const char* filepath, long& count) {
    FILE* file = fopen(filepath, "r");
    if (file == nullptr) {
        count = 0;
        return nullptr;
    }
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    count = size / sizeof(GpsPosition);
    GpsPosition* array = new GpsPosition[count];
    fread(array, sizeof(GpsPosition), count, file);
    return array; //mazat to nejdeme ;)
}

void writeToFileAsText(FILE* file, const char* format, ...) {
    va_list args;
    va_start (args, format);
    vfprintf (file, format, args);
    va_end (args);
}



int main(int argc, const char * argv[]) {
    //prvy parameter je vzdy cesta k binarke. dalsi sa caka nazov suboru bez pripony. tj. napr. 1589702610 (co je nazov suborov, lisi sa len koncovka s raw datami)
    //cize pouzitie by bolo: dataConverter 1589702610
    //a vyrobi to v rovnakej ceste ako su zdrojove subory, subor 1589702610.csv
    if (argc < 2) {
        cout<<"Usage: dataConverter filename_without_extension\nExample: dataConverter 1589702610\nAfter conversion 1589702610.csv is created in path of source files.";
    }
    
    std::string filepath(argv[1]);
    
    long accCount = 0;
    Double4* accData = loadAccGyro((filepath + ".acc").c_str(), accCount);
    accData = movingAverage(accData, accCount);
    long gyroCount = 0;
    Double4* gyroData = loadAccGyro((filepath + ".gyro").c_str(), gyroCount);
    gyroData = movingAverage(gyroData, gyroCount);
    long gpsCount = 0;
    GpsPosition* gpsData = loadGps((filepath + ".gps").c_str(), gpsCount);
    long altiCount = 0;
    Altimeter* altiData = loadAltimeter((filepath + ".alti").c_str(), altiCount);
    
    
    double maxDouble = numeric_limits<double>::max();
    double accTimestamp=maxDouble, gyroTimestamp=maxDouble, gpsTimestamp=maxDouble, altiTimestamp=maxDouble;
    //najdi najmensi timestamp
    if (accCount > 0) {
        accTimestamp = accData[0].timestamp;
    }
    if(gyroCount > 0) {
        gyroTimestamp = gyroData[0].timestamp;
    }
    if(gpsCount > 0) {
        gpsTimestamp = gpsData[0].timestamp;
    }
    if(altiCount > 0) {
        altiTimestamp = altiData[0].timestamp;
    }
    
    double minTimestamp = accTimestamp < gyroTimestamp ? accTimestamp : gyroTimestamp;
    minTimestamp = minTimestamp < gpsTimestamp ? minTimestamp : gpsTimestamp;
    minTimestamp = minTimestamp < altiTimestamp ? minTimestamp : altiTimestamp;
    
    long accIndex = 0;
    long gyroIndex = 0;
    long gpsIndex = 0;
    long altiIndex = 0;
    
    FILE* writeFP = fopen((filepath + ".csv").c_str(), "w");
    double simulationTime = 0;
    double simulationTimestep = 0.02;
    
    double lastGpsSpeed = 0;
    double lastPressure = 0;
    Double4 lastAcc;
    Double4 lastGyro;
    
    double zeroTime = minTimestamp;
    
    writeToFileAsText(writeFP, "timestamp,accx,accy,accz,gyrox,gyroy,gyroz,gpsspeed,pressure\n");
    while(true) {
        if (accIndex >= accCount && gyroIndex >= gyroCount && gpsIndex >= gpsCount && altiIndex >= altiCount) {
            break; //ende
        }
        
        bool sendAcc = false;
        bool sendGyro = false;
        bool sendGPS = false;
        bool sendAlti = false;
        
        if(accIndex < accCount) {
            double time = accData[accIndex].timestamp - zeroTime;
            //NSLog(@"accTime: %.2f", time);
            if (time < simulationTime) {
                //send acc
                lastAcc = accData[accIndex];
                sendAcc = true;
                accIndex += 1;
            }
        }
        
        if (gyroIndex < gyroCount) {
            double time = gyroData[gyroIndex].timestamp - zeroTime;
            if (time < simulationTime) {
                lastGyro = gyroData[gyroIndex];
                sendGyro = true;
                gyroIndex+=1;
            }
        }
        
        if (gpsIndex < gpsCount) {
            double time = gpsData[gpsIndex].timestamp - zeroTime;
            if (time < simulationTime) {
                lastGpsSpeed = gpsData[gpsIndex].speed;
                sendGPS = true;
                gpsIndex+=1;
            }
        }
        
        if (altiIndex < altiCount) {
            double time = altiData[altiIndex].timestamp - zeroTime;
            if (time < simulationTime) {
                lastPressure = altiData[altiIndex].pressure;
                sendAlti=true;
                altiIndex+=1;
            }
        }
        
        writeToFileAsText(writeFP, "%f,%f,%f,%f,%f,%f,%f,%f,%f\n",simulationTime+zeroTime, lastAcc.x,lastAcc.y,lastAcc.z,lastGyro.x,lastGyro.y,lastGyro.z,lastGpsSpeed,lastPressure);
        
        simulationTime += simulationTimestep;
    };
    
    
    fclose(writeFP);
    
    return 0;
}
