/*
 * BatteryEKF.cpp - Extended Kalman Filter for Battery SOC Estimation
 * 
 * Implementation of 4-state EKF with Neural Network correction
 */

#include "BatteryEKF.h"
#include "ECM_LookupTables.h"
#include "NN_Weights.h"
#include <math.h>

// Helper macro to access 2D array stored as 1D (row-major)
#define MAT(arr, row, col, ncols) ((arr)[(row) * (ncols) + (col)])

// Constructor
BatteryEKF::BatteryEKF(float sample_time_s) 
    : Ts(sample_time_s)
    , adaptive_tuning_enabled(true)
    , nn_enabled(true)
    , V_predicted(0.0f)
    , voltage_error(0.0f)
    , parasitic_R(0.0f)           // Default: no parasitic resistance
    , max_gain_clamp(0.5f)         // Default: 0.5% per update
    , trust_guard_threshold(0.1f)   // Default: 100mV
{
    // Initialize state vector to zero
    for (int i = 0; i < EKF_STATE_DIM; i++) {
        x[i] = 0.0f;
    }
    
    // Initialize covariance matrix P to zero
    for (int i = 0; i < EKF_STATE_DIM * EKF_STATE_DIM; i++) {
        P[i] = 0.0f;
    }
    
    // Initialize process noise Q (diagonal)
    Q[0] = 1e-6f;   // SOC process noise
    Q[1] = 1e-3f;   // V_RC1 process noise
    Q[2] = 1e-3f;   // V_RC2 process noise
    Q[3] = 1e-3f;   // V_RC3 process noise
    
    // Initialize measurement noise R
    R = 0.005f * 0.005f;  // 5 mV standard deviation
}

// Initialize EKF
void BatteryEKF::begin(float initial_soc_pct) {
    // Set initial state
    x[0] = constrain(initial_soc_pct, 0.0f, 100.0f);
    x[1] = 0.0f;  // V_RC1 (assume relaxed)
    x[2] = 0.0f;  // V_RC2 (assume relaxed)
    x[3] = 0.0f;  // V_RC3 (assume relaxed)
    
    // Set initial covariance P (diagonal)
    for (int i = 0; i < EKF_STATE_DIM * EKF_STATE_DIM; i++) {
        P[i] = 0.0f;
    }
    
    // Initial uncertainties
    float P0_SOC = 1.0f;  // Crushed to 1% native uncertainty! (Since 2.43 Ohm parasite is stripped, model mathematically perfectly matches reality!)
    float P0_VRC = 1e-6f;   // Small uncertainty in RC voltages (relaxed)
    
    MAT(P, 0, 0, 4) = P0_SOC;
    MAT(P, 1, 1, 4) = P0_VRC;
    MAT(P, 2, 2, 4) = P0_VRC;
    MAT(P, 3, 3, 4) = P0_VRC;
    
    Serial.printf("[EKF] Initialized: SOC=%.1f%%, P0_SOC=%.1f\n", initial_soc_pct, P0_SOC);
}

// Reset to new SOC
void BatteryEKF::reset(float new_soc_pct) {
    x[0] = constrain(new_soc_pct, 0.0f, 100.0f);
    x[1] = 0.0f;
    x[2] = 0.0f;
    x[3] = 0.0f;
    
    // Reset covariance
    MAT(P, 0, 0, 4) = 1.0f;  // Trust perfectly physically stripped SOC mathematically explicitly (1%)
    MAT(P, 1, 1, 4) = 1e-6f;
    MAT(P, 2, 2, 4) = 1e-6f;
    MAT(P, 3, 3, 4) = 1e-6f;
}

// Get states
void BatteryEKF::getStates(float states[4]) const {
    for (int i = 0; i < 4; i++) {
        states[i] = x[i];
    }
}

// Get SOC uncertainty
float BatteryEKF::getSOCUncertainty() const {
    return sqrtf(MAT(P, 0, 0, 4));
}

// Interpolate lookup table
float BatteryEKF::interpolateLUT(const float* lut, float soc) {
    // Clamp SOC to valid range
    float soc_clamped = constrain(soc, 0.0f, 100.0f);
    
    // Linear interpolation
    int idx_low = (int)soc_clamped;
    int idx_high = idx_low + 1;
    
    if (idx_high >= LUT_SIZE) {
        idx_high = LUT_SIZE - 1;
        idx_low = idx_high - 1;
    }
    
    float frac = soc_clamped - (float)idx_low;
    
    // Read from PROGMEM
    float val_low = pgm_read_float(&lut[idx_low]);
    float val_high = pgm_read_float(&lut[idx_high]);
    
    return val_low + frac * (val_high - val_low);
}

// State transition function
void BatteryEKF::stateTransition(const float x_in[4], float current_A, 
                                 float x_out[4], float F[16]) {
    // Extract states
    float SOC = x_in[0];
    float V_RC1 = x_in[1];
    float V_RC2 = x_in[2];
    float V_RC3 = x_in[3];
    
    float SOC_clamped = constrain(SOC, 0.0f, 100.0f);
    
    // Get RC parameters from LUTs
    float R1 = interpolateLUT(LUT_RC1_R, SOC_clamped);
    float C1 = interpolateLUT(LUT_RC1_C, SOC_clamped);
    float R2 = interpolateLUT(LUT_RC2_R, SOC_clamped);
    float C2 = interpolateLUT(LUT_RC2_C, SOC_clamped);
    float R3 = interpolateLUT(LUT_RC3_R, SOC_clamped);
    float C3 = interpolateLUT(LUT_RC3_C, SOC_clamped);
    
    // Compute time constants (with minimum bound)
    float tau1 = max(R1 * C1, 1e-9f);
    float tau2 = max(R2 * C2, 1e-9f);
    float tau3 = max(R3 * C3, 1e-9f);
    
    // ZOH alpha values
    float a1 = expf(-Ts / tau1);
    float a2 = expf(-Ts / tau2);
    float a3 = expf(-Ts / tau3);
    
    // State transition equations
    x_out[0] = SOC + (current_A * Ts) / Q_NOMINAL_AS * 100.0f;  // SOC (%)
    x_out[1] = V_RC1 * a1 + R1 * (1.0f - a1) * current_A;       // V_RC1
    x_out[2] = V_RC2 * a2 + R2 * (1.0f - a2) * current_A;       // V_RC2
    x_out[3] = V_RC3 * a3 + R3 * (1.0f - a3) * current_A;       // V_RC3
    
    // Jacobian F = df/dx (4x4 diagonal for this model)
    for (int i = 0; i < 16; i++) F[i] = 0.0f;
    
    MAT(F, 0, 0, 4) = 1.0f;
    MAT(F, 1, 1, 4) = a1;
    MAT(F, 2, 2, 4) = a2;
    MAT(F, 3, 3, 4) = a3;
}

// Measurement function  
float BatteryEKF::measurementFunction(const float x_in[4], float current_A, float H[4]) {
    // Extract states
    float SOC = x_in[0];
    float V_RC1 = x_in[1];
    float V_RC2 = x_in[2];
    float V_RC3 = x_in[3];
    
    float SOC_clamped = constrain(SOC, 0.0f, 100.0f);
    
    // OCV lookup (with hysteresis)
    float OCV;
    if (current_A > 0.02f) {
        // Charging - use charge curve
        OCV = interpolateLUT(LUT_OCV_Charge, SOC_clamped);
    } else if (current_A < -0.02f) {
        // Discharging - use discharge curve
        OCV = interpolateLUT(LUT_OCV_Discharge, SOC_clamped);
    } else {
        // At Rest - use average of charge and discharge curves
        float OCV_chg = interpolateLUT(LUT_OCV_Charge, SOC_clamped);
        float OCV_dsg = interpolateLUT(LUT_OCV_Discharge, SOC_clamped);
        OCV = (OCV_chg + OCV_dsg) * 0.5f;
    }
    
    // R0 lookup
    float R0 = interpolateLUT(LUT_R0_eff, SOC_clamped); // PERFECT BARE CELL R0
    
    // Neural Network correction (if enabled and current > threshold)
    float V_NN = 0.0f;
    if (nn_enabled && fabsf(current_A) > 0.01f) {
        V_NN = neuralNetworkCorrection(SOC_clamped, current_A, V_RC2, V_RC3, OCV);
    }
    
    // Predicted terminal voltage
    float V_pred = OCV + R0 * current_A + V_RC1 + V_RC2 + V_RC3 + V_NN;
    
    // Compute Jacobian H = dh/dx (1x4)
    // dV/dSOC = dOCV/dSOC + dR0/dSOC * I (finite difference)
    float delta_SOC = 0.5f;
    float SOC_hi = min(100.0f, SOC_clamped + delta_SOC);
    float SOC_lo = max(0.0f, SOC_clamped - delta_SOC);
    
    float OCV_hi, OCV_lo;
    if (current_A > 0.02f) {
        OCV_hi = interpolateLUT(LUT_OCV_Charge, SOC_hi);
        OCV_lo = interpolateLUT(LUT_OCV_Charge, SOC_lo);
    } else if (current_A < -0.02f) {
        OCV_hi = interpolateLUT(LUT_OCV_Discharge, SOC_hi);
        OCV_lo = interpolateLUT(LUT_OCV_Discharge, SOC_lo);
    } else {
        float OCV_chg_hi = interpolateLUT(LUT_OCV_Charge, SOC_hi);
        float OCV_dsg_hi = interpolateLUT(LUT_OCV_Discharge, SOC_hi);
        OCV_hi = (OCV_chg_hi + OCV_dsg_hi) * 0.5f;
        
        float OCV_chg_lo = interpolateLUT(LUT_OCV_Charge, SOC_lo);
        float OCV_dsg_lo = interpolateLUT(LUT_OCV_Discharge, SOC_lo);
        OCV_lo = (OCV_chg_lo + OCV_dsg_lo) * 0.5f;
    }
    
    float R0_hi = interpolateLUT(LUT_R0_eff, SOC_hi);
    float R0_lo = interpolateLUT(LUT_R0_eff, SOC_lo);
    
    float dV_dSOC = (OCV_hi - OCV_lo) / (SOC_hi - SOC_lo) + 
                    (R0_hi - R0_lo) / (SOC_hi - SOC_lo) * current_A;
    
    H[0] = dV_dSOC;
    H[1] = 1.0f;  // dV/dV_RC1
    H[2] = 1.0f;  // dV/dV_RC2
    H[3] = 1.0f;  // dV/dV_RC3
    
    return V_pred;
}

// Neural Network forward pass
float BatteryEKF::neuralNetworkCorrection(float soc, float current, 
                                          float V_RC2, float V_RC3, float ocv) {
    // Input features: [SOC, Current, V_RC2, V_RC3, Current_Sign, OCV]
    float x_nn[NN_INPUT_SIZE];
    x_nn[0] = soc;
    x_nn[1] = current;
    x_nn[2] = V_RC2;
    x_nn[3] = V_RC3;
    x_nn[4] = (current >= 0.0f) ? 1.0f : -1.0f;  // Current sign
    x_nn[5] = ocv;
    
    // Normalize inputs
    for (int i = 0; i < NN_INPUT_SIZE; i++) {
        float mean = pgm_read_float(&NN_feat_means[i]);
        float std = pgm_read_float(&NN_feat_stds[i]);
        x_nn[i] = (x_nn[i] - mean) / std;
    }
    
    // Layer 1: h1 = tanh(W1 * x + b1)
    float h1[NN_HIDDEN1_SIZE];
    for (int i = 0; i < NN_HIDDEN1_SIZE; i++) {
        float sum = pgm_read_float(&NN_b1[i]);
        for (int j = 0; j < NN_INPUT_SIZE; j++) {
            float w = pgm_read_float(&NN_W1[i * NN_INPUT_SIZE + j]);
            sum += w * x_nn[j];
        }
        h1[i] = tanhf(sum);
    }
    
    // Layer 2: h2 = tanh(W2 * h1 + b2)
    float h2[NN_HIDDEN2_SIZE];
    for (int i = 0; i < NN_HIDDEN2_SIZE; i++) {
        float sum = pgm_read_float(&NN_b2[i]);
        for (int j = 0; j < NN_HIDDEN1_SIZE; j++) {
            float w = pgm_read_float(&NN_W2[i * NN_HIDDEN1_SIZE + j]);
            sum += w * h1[j];
        }
        h2[i] = tanhf(sum);
    }
    
    // Layer 3: h3 = tanh(W3 * h2 + b3)
    float h3[NN_HIDDEN3_SIZE];
    for (int i = 0; i < NN_HIDDEN3_SIZE; i++) {
        float sum = pgm_read_float(&NN_b3[i]);
        for (int j = 0; j < NN_HIDDEN2_SIZE; j++) {
            float w = pgm_read_float(&NN_W3[i * NN_HIDDEN2_SIZE + j]);
            sum += w * h2[j];
        }
        h3[i] = tanhf(sum);
    }
    
    // Output layer: y = W4 * h3 + b4 (linear)
    float y_norm = pgm_read_float(&NN_b4[0]);
    for (int j = 0; j < NN_HIDDEN3_SIZE; j++) {
        float w = pgm_read_float(&NN_W4[j]);
        y_norm += w * h3[j];
    }
    
    // Denormalize output
    float V_NN = y_norm * NN_target_std + NN_target_mean;
    
    return V_NN;
}

// Adaptive Q/R tuning based on voltage error and SOC uncertainty
void BatteryEKF::adaptQR(float current_A, float voltage_err, float soc_uncertainty) {
    if (!adaptive_tuning_enabled) {
        // Fixed tuning
        Q[0] = 1e-6f;
        Q[1] = 1e-3f;
        Q[2] = 1e-3f;
        Q[3] = 1e-3f;
        R = 0.005f * 0.005f;  // 5 mV variance
        return;
    }
    
    // Adaptive tuning logic from MATLAB validation script
    float SOC_current = x[0];
    
    bool is_at_rest = (fabsf(current_A) < 0.02f); // Less than 20mA is rest

    float abs_v_err = fabsf(voltage_err);
    
    // FATAL FLAW FIX: The EKF was punishing SOC during high current because of unmodeled IR drop!
    // We MUST ONLY trust the voltage heavily when AT REST.
    if (is_at_rest) {
        // At rest, voltage is close to true OCV. If we have high error, slowly drag SOC.
        if (abs_v_err > 0.050f && SOC_current > 10.0f) {  
            Q[0] = 5.0f;    
            R = 0.0001f;    
        }
        else if (abs_v_err > 0.020f && SOC_current > 10.0f) {
            Q[0] = 1.0f;
            R = 0.001f;
        }
        else if (abs_v_err > 0.005f && SOC_current > 10.0f) {
            Q[0] = 0.1f * Ts;
            R = 0.01f;
        }
        else {
            Q[0] = 1e-8f; // Minimal SOC drift at rest
            R = 0.0001f;  // Lock onto good voltage
        }
    } 
    else {
        // ACTIVE LOAD: The user's Neural Network is specifically designed for this phase!
        // We restore Q and R so the filter actively relies on and trusts the ECM+NN model!
        Q[0] = 5.0f;     // High process noise (actively tracking NN model)
        R = 0.001f;      // Very low measurement noise (heavily trust the voltage/NN relationship)
        
        // We let the microscopic mathematical 'Kalman Gain Clamp' protect from the 80% wiring spikes! 
    }
    
    // Apply to all RC diagonal elements
    Q[1] = 0.01f;
    Q[2] = 0.01f;
    Q[3] = 0.01f;
}
// Main EKF update function
void BatteryEKF::update(float current_A, float voltage_V) {
    // ========================================================================
    // PRE-PROCESSING: REMOVE PARASITIC RESISTANCE
    // ========================================================================
    // The testboard wiring introduce a parasitic voltage drop (V_batt = V_raw - I*R)
    // Positive current = Charging (adds to raw V), Negative = Discharging (subs from raw V)
    if (parasitic_R > 0.001f && fabsf(current_A) > 0.05f) {
        voltage_V = voltage_V - (current_A * parasitic_R);
    }

    // ========================================================================
    // ADAPTIVE TUNING
    // ========================================================================
    adaptQR(current_A, voltage_error, getSOCUncertainty());

    // ========================================================================
    // PREDICTION STEP
    // ========================================================================

    float x_pred[4];
    float F[16];  // Jacobian df/dx

    stateTransition(x, current_A, x_pred, F);
    
    // Predict covariance: P_pred = F * P * F' + Q
    float P_pred[16];
    float FP[16];
    float FP_FT[16];
    float F_T[16];
    
    matrixTranspose4x4(F, F_T);
    matrixMultiply4x4(F, P, FP);
    matrixMultiply4x4(FP, F_T, FP_FT);
    
    // Add process noise Q (diagonal)
    for (int i = 0; i < 4; i++) {
        MAT(P_pred, i, i, 4) = MAT(FP_FT, i, i, 4) + Q[i];
        for (int j = 0; j < 4; j++) {
            if (i != j) {
                MAT(P_pred, i, j, 4) = MAT(FP_FT, i, j, 4);
            }
        }
    }
    
    // ========================================================================
    // MEASUREMENT UPDATE STEP
    // ========================================================================
    
    float H[4];  // Jacobian dh/dx (1x4)
    V_predicted = measurementFunction(x_pred, current_A, H);
    voltage_error = fabsf(voltage_V - V_predicted);
    
    // EKF Trust Guard: If voltage error is HUGE (pulse load), increase R to reduce trust in model
    float final_R = R;
    if (voltage_error > trust_guard_threshold) {
        final_R *= 10.0f; // Increase measurement noise factor
    }

    // Innovation covariance: S = H * P_pred * H' + final_R
    float HP[4];  // H * P_pred (1x4)
    for (int j = 0; j < 4; j++) {
        HP[j] = 0.0f;
        for (int k = 0; k < 4; k++) {
            HP[j] += H[k] * MAT(P_pred, k, j, 4);
        }
    }
    
    float S = final_R;  // scalar
    for (int j = 0; j < 4; j++) {
        S += HP[j] * H[j];
    }
    
    // Kalman gain: K = P_pred * H' / S (4x1)
    float K[4];
    for (int i = 0; i < 4; i++) {
        float PH = 0.0f;
        for (int j = 0; j < 4; j++) {
            PH += MAT(P_pred, i, j, 4) * H[j];
        }
        K[i] = PH / S;
    }
    
    // SOC Gain clamping to prevent rapid jumps
    if (K[0] > max_gain_clamp) {
        K[0] = max_gain_clamp;
    } else if (K[0] < -max_gain_clamp) {
        K[0] = -max_gain_clamp;
    }
    
    // Update state: x = x_pred + K * (y - y_pred)
    float innovation = voltage_V - V_predicted;
    for (int i = 0; i < 4; i++) {
        x[i] = x_pred[i] + K[i] * innovation;
    }
    
    // Constrain SOC to valid range
    x[0] = constrain(x[0], 0.0f, 100.0f);
    
    // Update covariance: P = (I - K*H) * P_pred
    float KH[16];  // K * H (4x4)
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            MAT(KH, i, j, 4) = K[i] * H[j];
        }
    }
    
    float I_KH[16];  // I - K*H
    for (int i = 0; i < 16; i++) I_KH[i] = 0.0f;
    for (int i = 0; i < 4; i++) {
        MAT(I_KH, i, i, 4) = 1.0f;
    }
    for (int i = 0; i < 16; i++) {
        I_KH[i] -= KH[i];
    }
    
    matrixMultiply4x4(I_KH, P_pred, P);
    
#ifdef EKF_DEBUG
    Serial.printf("EKF: SOC=%.1f%%, unc=%.1f%%, V_err=%.1fmV, Q[0]=%.2f, R=%.5f\n",
                  x[0], 
                  sqrtf(fabsf(P[0][0])),
                  voltage_error * 1000.0f,
                  Q[0],
                  R);
#endif
}

// ============================================================================
// MATRIX OPERATIONS (4x4)
// ============================================================================

void BatteryEKF::matrixMultiply4x4(const float* A, const float* B, float* C) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++) {
                sum += MAT(A, i, k, 4) * MAT(B, k, j, 4);
            }
            MAT(C, i, j, 4) = sum;
        }
    }
}

void BatteryEKF::matrixAdd4x4(const float* A, const float* B, float* C) {
    for (int i = 0; i < 16; i++) {
        C[i] = A[i] + B[i];
    }
}

void BatteryEKF::matrixTranspose4x4(const float* A, float* AT) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            MAT(AT, j, i, 4) = MAT(A, i, j, 4);
        }
    }
}

// Helper: Binary search through OCV discharge table
float BatteryEKF::invertOCV_Discharge(float voltage) {
    // Clamp voltage to valid range
    float v_min = LUT_OCV_Discharge[0];    // 2.586V
    float v_max = LUT_OCV_Discharge[100];  // 4.169V
    
    if (voltage <= v_min) return 0.0f;
    if (voltage >= v_max) return 100.0f;
    
    // Linear search through LUT (101 points, fast enough)
    for (int soc = 0; soc < 100; soc++) {
        float v_low = LUT_OCV_Discharge[soc];
        float v_high = LUT_OCV_Discharge[soc + 1];
        
        if (voltage >= v_low && voltage <= v_high) {
            // Linear interpolation between points
            float frac = (voltage - v_low) / (v_high - v_low);
            return (float)soc + frac;
        }
    }
    
    // Fallback (shouldn't reach here)
    return 50.0f;
}
