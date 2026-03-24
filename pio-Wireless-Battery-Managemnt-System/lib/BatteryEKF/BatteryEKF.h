/*
 * BatteryEKF.h - Extended Kalman Filter for Battery SOC Estimation
 * 
 * Samsung 30Q 18650 Li-ion Battery Model
 * 4-state Equivalent Circuit Model with Neural Network Correction
 * 
 * State Vector: x = [SOC, V_RC1, V_RC2, V_RC3]
 *   - SOC: State of Charge (%)
 *   - V_RC1: Voltage across RC1 (fast RC, τ~0.007-0.009s)
 *   - V_RC2: Voltage across RC2 (slow diffusion, τ~27-66s)
 *   - V_RC3: Voltage across RC3 (Warburg fast, τ~109-720s)
 * 
 * Measurement: V_terminal (V)
 * Input: Current (A), negative = discharge, positive = charge
 * 
 * Author: Auto-generated from MATLAB EKF implementation
 * Battery: Samsung 30Q, 3.043 Ah nominal capacity
 */

#ifndef BATTERY_EKF_H
#define BATTERY_EKF_H

#include <Arduino.h>

// Battery specifications
#define Q_NOMINAL_AH 3.043f      // Nominal capacity (Ah)
#define Q_NOMINAL_AS (Q_NOMINAL_AH * 3600.0f)  // Capacity in As

// EKF dimensions
#define EKF_STATE_DIM 4          // [SOC, V_RC1, V_RC2, V_RC3]
#define EKF_MEAS_DIM 1           // [V_terminal]
#define EKF_INPUT_DIM 1          // [Current]

// Adaptive tuning thresholds
#define VOLTAGE_ERROR_HIGH 0.025f    // 25 mV
#define VOLTAGE_ERROR_MED  0.012f    // 12 mV
#define VOLTAGE_ERROR_LOW  0.005f    // 5 mV
#define SOC_UNCERTAINTY_THRESHOLD 2.0f  // 2% SOC uncertainty
#define LOW_SOC_THRESHOLD 10.0f      // Below 10% SOC trust coulomb counting

class BatteryEKF {
public:
    // Constructor
    BatteryEKF(float sample_time_s = 10.0f);
    
    // Initialize EKF with SOC estimate
    void begin(float initial_soc_pct = 100.0f);
    
    // Main EKF update function
    // current_A: positive = charge, negative = discharge
    // voltage_V: measured terminal voltage
    void update(float current_A, float voltage_V);
    
    // Get current SOC estimate
    float getSOC() const { return x[0]; }
    
    // Get SOC uncertainty (standard deviation)
    float getSOCUncertainty() const;
    
    // Get estimated terminal voltage
    float getEstimatedVoltage() const { return V_predicted; }
    
    // Get voltage error
    float getVoltageError() const { return voltage_error; }
    
    // Get all states [SOC, V_RC1, V_RC2, V_RC3]
    void getStates(float states[4]) const;
    
    // Enable/disable adaptive tuning
    void setAdaptiveTuning(bool enable) { adaptive_tuning_enabled = enable; }
    
    // Enable/disable Neural Network correction
    void setNNEnabled(bool enable) { nn_enabled = enable; }
    
    // Reset EKF to new SOC
    void reset(float new_soc_pct);

    // State of charge (SOC) from OCV
    float invertOCV_Discharge(float voltage);
    
    // Configuration Setters
    void setParasiticResistance(float resistance_ohms) { parasitic_R = resistance_ohms; }
    void setMaxGainClamp(float max_pct_per_update) { max_gain_clamp = max_pct_per_update; }
    void setTrustGuardThreshold(float voltage_err_v) { trust_guard_threshold = voltage_err_v; }
    
    // Get diagnostic info
    float getCurrentQ() const { return Q[0]; }  // Current process noise for SOC
    float getCurrentR() const { return R; }      // Current measurement noise
    float getParasiticR() const { return parasitic_R; }
    
private:
    // Sample time
    float Ts;
    
    // State vector: [SOC(%), V_RC1(V), V_RC2(V), V_RC3(V)]
    float x[EKF_STATE_DIM];
    
    // Covariance matrix P (4x4, stored as 1D array row-major)
    float P[EKF_STATE_DIM * EKF_STATE_DIM];
    
    // Process noise covariance Q (diagonal)
    float Q[EKF_STATE_DIM];
    
    // Measurement noise covariance R (scalar)
    float R;
    
    // Predicted voltage
    float V_predicted;
    
    // Voltage error
    float voltage_error;
    
    // Flags
    bool adaptive_tuning_enabled;
    bool nn_enabled;
    
    // Configuration parameters
    float parasitic_R;
    float max_gain_clamp;
    float trust_guard_threshold;
    
    // Internal functions
    void stateTransition(const float x_in[4], float current_A, float x_out[4], float F[16]);
    float measurementFunction(const float x_in[4], float current_A, float H[4]);
    float neuralNetworkCorrection(float soc, float current, float V_RC2, float V_RC3, float ocv);
    float interpolateLUT(const float* lut, float soc);
    void adaptQR(float current_A, float voltage_err, float soc_uncertainty);
    
    // Matrix operations (4x4 matrices)
    void matrixMultiply4x4(const float* A, const float* B, float* C);
    void matrixAdd4x4(const float* A, const float* B, float* C);
    void matrixTranspose4x4(const float* A, float* AT);
};

#endif // BATTERY_EKF_H
