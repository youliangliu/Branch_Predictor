//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include <string.h>
#include "predictor.h"

//
// TODO:Student Information
//
const char *studentName = "Youliang Liu";
const char *studentID   = "A14003277";
const char *email       = "yol117@ucsd.edu";

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = { "Static", "Gshare",
                          "Tournament", "Custom" };

int ghistoryBits = 12; // Number of bits used for Global History
int lhistoryBits; // Number of bits used for Local History
int pcIndexBits;  // Number of bits used for PC index
int bpType;       // Branch Prediction Type
int verbose;

//int customBits = 13;
//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

//
//TODO: Add your own Branch Predictor data structures here
//

uint32_t gshareHR;
uint8_t *gshareBranchHT;


uint32_t *localPatternHT;
uint8_t *localBranchHT;
uint32_t globalHR;
uint8_t *globalBranchHT;
uint8_t *chooser;

uint8_t gsharePrediction;
uint8_t globalPrediction;
uint8_t localPrediction;
uint8_t tournamentPrediction;

//Custom Variables
uint32_t customHR_1;
uint32_t customHR_2;
uint32_t customHR_3;

uint8_t *customBranchHT_1;
uint8_t *customBranchHT_2;
uint8_t *customBranchHT_3;


void update_prediction(uint8_t* oldPrediction, uint8_t outcome){
    if(outcome == NOTTAKEN && *oldPrediction != SN){
        (*oldPrediction)--;
    }
    else if(outcome == TAKEN && *oldPrediction != ST){
        (*oldPrediction)++;
    }
}

//Initialize custom Skewed branch Predictor
//Inspired by Skewed branch predictors paper by Pierre Michaud, Andre Seznec, Richard Uhlig
void init_skewed_predictor(){
    customHR_1 = 0;
    customHR_2 = 0;
    customHR_3 = 0;

    customBranchHT_1 = malloc((1<<(ghistoryBits+4)) * sizeof(uint8_t));
    customBranchHT_2 = malloc((1<<(ghistoryBits+4)) * sizeof(uint8_t));
    customBranchHT_3 = malloc((1<<(ghistoryBits+4)) * sizeof(uint8_t));

    memset(customBranchHT_1, WN, (1<<(ghistoryBits+4)) * sizeof(uint8_t));
    memset(customBranchHT_2, WN, (1<<(ghistoryBits+4)) * sizeof(uint8_t));
    memset(customBranchHT_3, WN, (1<<(ghistoryBits+4)) * sizeof(uint8_t));
}

uint32_t H(uint32_t input){
    uint32_t LSB = input & 1;
    uint32_t MSB = 0;   // this variable will represent the MSB we're looking for
    uint32_t tmp = input;
    // sizeof(unsigned int) = 4 (in Bytes)
    // 1 Byte = 8 bits
    // So 4 Bytes are 4 * 8 = 32 bits
    // We have to perform a right shift 32 times to have the
    // MSB in the LSB position.
    for (int i = sizeof(unsigned int) * 8; i > 0; i--) {

        MSB = (tmp & 1); // in the last iteration this contains the MSB value

        tmp >>= 1; // perform the 1-bit right shift
    }

    uint32_t newBit = MSB ^ LSB;
    //printf("OLD input: %x\n", input);
    input >>= 1;
    //printf("NEW input: %x\n", input);
    newBit <<= 31;
    //printf("newBit: %x\n", newBit);
    input = input ^ newBit;
    return input;
}

uint32_t H_inverse(uint32_t input){
    uint32_t LSB = input & 1;
    uint32_t MSB = 0;   // this variable will represent the MSB we're looking for
    uint32_t tmp = input;
    // sizeof(unsigned int) = 4 (in Bytes)
    // 1 Byte = 8 bits
    // So 4 Bytes are 4 * 8 = 32 bits
    // We have to perform a right shift 32 times to have the
    // MSB in the LSB position.
    for (int i = sizeof(unsigned int) * 8; i > 0; i--) {

        MSB = (tmp & 1); // in the last iteration this contains the MSB value

        tmp >>= 1; // perform the 1-bit right shift
    }

    //printf("MSB: %d\n", MSB);
    //printf("LSB: %d\n", LSB);

    uint32_t newBit = MSB ^ LSB;
    input <<= 1;
    input = input ^ newBit;
    return input;
}

uint32_t hashFunction_1(uint32_t pc, uint32_t HR){
    return H(pc) ^ H_inverse(HR) ^ HR;
}

uint32_t hashFunction_2(uint32_t pc, uint32_t HR){
    return H(pc) ^ H_inverse(HR) ^ pc;
}

uint32_t hashFunction_3(uint32_t pc, uint32_t HR){
    return H_inverse(pc) ^ H(HR) ^ pc;
}

uint8_t get_custom_prediction(uint32_t pc){
    uint32_t BHTindex_1 = hashFunction_1(pc, customHR_1) & ((1<<(ghistoryBits+4)) - 1);
    uint32_t BHTindex_2 = hashFunction_2(pc, customHR_2) & ((1<<(ghistoryBits+4)) - 1);
    uint32_t BHTindex_3 = hashFunction_3(pc, customHR_3) & ((1<<(ghistoryBits+4)) - 1);
    uint8_t prediction_1 = customBranchHT_1[BHTindex_1];
    uint8_t prediction_2 = customBranchHT_2[BHTindex_2];
    uint8_t prediction_3 = customBranchHT_3[BHTindex_3];
    uint8_t new_prediction_1 = (prediction_1 == WN || prediction_1 == SN) ? NOTTAKEN : TAKEN;
    uint8_t new_prediction_2 = (prediction_2 == WN || prediction_2 == SN) ? NOTTAKEN : TAKEN;
    uint8_t new_prediction_3 = (prediction_3 == WN || prediction_3 == SN) ? NOTTAKEN : TAKEN;
    uint8_t total_prediction = new_prediction_1 + new_prediction_2 + new_prediction_3;
    return (total_prediction > 2) ? TAKEN : NOTTAKEN;
}

void train_custom_predictor(uint32_t pc, uint8_t outcome){
    update_prediction(&customBranchHT_1[hashFunction_1(pc, customHR_1) & ((1 << (ghistoryBits+4)) - 1)], outcome);
    customHR_1 <<= 1;
    customHR_1 |= outcome;

    update_prediction(&customBranchHT_2[hashFunction_2(pc, customHR_2) & ((1 << (ghistoryBits+4)) - 1)], outcome);
    customHR_2 <<= 1;
    customHR_2 |= outcome;

    update_prediction(&customBranchHT_3[hashFunction_3(pc, customHR_3) & ((1 << (ghistoryBits+4)) - 1)], outcome);
    customHR_3 <<= 1;
    customHR_3 |= outcome;
}


//Initialize Global History predictor
void init_gshareHistory_predictor(){
    //Initialize GHR to be zero
    gshareHR = 0;
    //Initialize global branch history table to be all WN
    gshareBranchHT = malloc((1<<ghistoryBits) * sizeof(uint8_t));
    memset(gshareBranchHT, WN, (1<<ghistoryBits) * sizeof(uint8_t));
}

//Initialize Tournament Predictor
void init_tournament_predictor(){
    //Initialize local pattern history table to be all NOTTAKEN
    localPatternHT = (uint32_t*)malloc((1<<pcIndexBits) * sizeof(uint32_t));
    memset(localPatternHT, 0, (1<<pcIndexBits) * sizeof(uint32_t));
    //Initialize local branch history table to be all Weakly Not Taken
    localBranchHT = (uint8_t*)malloc((1<<lhistoryBits) * sizeof(uint8_t));
    memset(localBranchHT, WN, (1<<lhistoryBits) * sizeof(uint8_t));
    //Initialize GHR to be zero
    globalHR = 0;
    //Initialize global branch history table to be all WN
    globalBranchHT = (uint8_t*)malloc((1<<ghistoryBits) * sizeof(uint8_t));
    memset(globalBranchHT, WN, (1<<ghistoryBits) * sizeof(uint8_t));
    //Initialize chooser to weakly select global predictor, WN means Weakly Nottaken the Local prediction
    chooser = (uint8_t*)malloc((1<<ghistoryBits) * sizeof(uint8_t));
    memset(chooser, WN, (1<<ghistoryBits) * sizeof(uint8_t));
}

//------------------------------------//
//        Get Predictions             //
//------------------------------------//


uint8_t get_local_prediction(uint32_t pc){
    uint32_t PHTindex = pc & ((1<<pcIndexBits) - 1);
    uint32_t BHTindex = localPatternHT[PHTindex];
    uint8_t prediction = localBranchHT[BHTindex];
    localPrediction = (prediction == WN || prediction == SN) ? NOTTAKEN : TAKEN;
    return localPrediction;
}

uint8_t get_gshare_prediction(uint32_t pc){
    uint32_t BHTindex = (gshareHR ^ pc) & ((1<<ghistoryBits) - 1);
    uint8_t prediction = gshareBranchHT[BHTindex];
    gsharePrediction = (prediction == WN || prediction == SN) ? NOTTAKEN : TAKEN;
    //printf("global Prediction is: %02hhX\n", globalPrediction);
    return gsharePrediction;
}

uint8_t get_global_prediction(uint32_t pc){
    uint32_t BHTindex = (globalHR) & ((1<<ghistoryBits) - 1);
    uint8_t prediction = globalBranchHT[BHTindex];
    globalPrediction = (prediction == WN || prediction == SN) ? NOTTAKEN : TAKEN;
    //printf("global Prediction is: %02hhX\n", globalPrediction);
    return globalPrediction;
}


uint8_t get_tournament_prediction(uint32_t pc){
    uint8_t localResult = get_local_prediction(pc);
    uint8_t globalResult = get_global_prediction(pc);
    uint32_t chooserIndex = (globalHR) & ((1<<ghistoryBits) - 1);
    uint8_t predictorChoice = chooser[chooserIndex];
    tournamentPrediction = (predictorChoice == WN || predictorChoice == SN) ? globalResult : localResult;
    return tournamentPrediction;
}


//------------------------------------//
//        Train Predictors            //
//------------------------------------//


void train_local_predictor(uint32_t pc, uint8_t outcome){
    //get_local_prediction(pc);
    uint32_t PHTindex = pc & ((1<<pcIndexBits) - 1);
    uint32_t BHTindex = localPatternHT[PHTindex];
    update_prediction(&localBranchHT[BHTindex], outcome);
    localPatternHT[PHTindex] <<= 1;
    localPatternHT[PHTindex] &= ((1 << lhistoryBits) - 1);
    localPatternHT[PHTindex] |= outcome;
    return;
}

void train_gshare_predictor(uint32_t pc, uint8_t outcome){
    //get_global_prediction(pc);
    //uint8_t BHTindex = (pc ^ globalHR) & ((1<<ghistoryBits) - 1);
    //uint8_t* oldPrediction = &globalBranchHT[BHTindex];
    //printf("Old: %02hhX\n", *oldPrediction);
    //update_prediction(oldPrediction, outcome);
    //printf("New: %02hhX\n", *oldPrediction);
    //globalHR <<= 1;
    //globalHR &= ((1 << ghistoryBits) - 1);
    //globalHR |= outcome;

    update_prediction(&gshareBranchHT[(gshareHR ^ pc) & ((1 << ghistoryBits) - 1)], outcome);
    //printf("New: %02hhX\n", *oldPrediction);
    gshareHR <<= 1;
    gshareHR |= outcome;
    return;
}

void train_global_predictor(uint32_t pc, uint8_t outcome){
    update_prediction(&globalBranchHT[(globalHR) & ((1 << ghistoryBits) - 1)], outcome);
    globalHR <<= 1;
    globalHR &= ((1 << ghistoryBits) - 1);
    globalHR |= outcome;
    return;
}

void train_tournament_predictor(uint32_t pc, uint8_t outcome){
    //get_tournament_prediction(pc);
    if(localPrediction != globalPrediction){
        update_prediction(&chooser[globalHR],
                          (globalPrediction == outcome) ? NOTTAKEN : TAKEN);
    }
    train_local_predictor(pc, outcome);
    train_global_predictor(pc, outcome);
    return;
}



//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
//
void init_predictor()
{
    //
    //TODO: Initialize Branch Predictor Data Structures
    //
    if(bpType == STATIC){
        return;
    }
    else if(bpType == GSHARE){
        init_gshareHistory_predictor();
        return;
    }
    else if(bpType == TOURNAMENT){
        init_tournament_predictor();
        return;
    }
    else if(bpType == CUSTOM){
        init_skewed_predictor();
        /*
        uint32_t a = 0xF0000000;
        printf("a: %x\n", a);
        uint32_t b = H(a);
        printf("H(a): %x\n", b);
        uint32_t c = H_inverse(a);
        printf("H_inverse(a): %x\n", c);
         */
        return;
    }
    return;
}



// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint8_t make_prediction(uint32_t pc)
{
    //
    //TODO: Implement prediction scheme
    //

    // Make a prediction based on the bpType
    switch (bpType) {
        case STATIC:
            return TAKEN;
        case GSHARE:
            //printf("Here");
            return get_gshare_prediction(pc);
        case TOURNAMENT:
            return get_tournament_prediction(pc);
        case CUSTOM:
            return get_custom_prediction(pc);
        default:
            break;
    }

    // If there is not a compatable bpType then return NOTTAKEN
    return NOTTAKEN;
}


// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//
void train_predictor(uint32_t pc, uint8_t outcome)
{
    //
    //TODO: Implement Predictor training
    //
    if(bpType == STATIC){
        return;
    }
    else if(bpType == GSHARE){
        train_gshare_predictor(pc, outcome);
        return;
    }
    else if(bpType == TOURNAMENT){
        train_tournament_predictor(pc, outcome);
        return;
    }
    else if(bpType == CUSTOM){
        train_custom_predictor(pc, outcome);
        return;
    }
    return;
}



