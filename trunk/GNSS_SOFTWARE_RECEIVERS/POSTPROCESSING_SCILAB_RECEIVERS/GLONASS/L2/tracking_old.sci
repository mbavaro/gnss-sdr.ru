function [trackResults, channel]= tracking(fid, channel, settings)
// Performs code and carrier tracking for all channels.
//
//[trackResults, channel] = tracking(fid, channel, settings)
//
//   Inputs:
//       fid             - file identifier of the signal record.
//       channel         - PRN, carrier frequencies and code phases of all
//                       satellites to be tracked (prepared by preRum.m from
//                       acquisition results).
//       settings        - receiver settings.
//   Outputs:
//       trackResults    - tracking results (structure array). Contains
//                       in-phase prompt outputs and absolute spreading
//                       code's starting positions, together with other
//                       observation data from the tracking loops. All are
//                       saved every millisecond.

//--------------------------------------------------------------------------
//                           SoftGNSS v3.0
// 
// Copyright (C) Dennis M. Akos
// Written by Darius Plausinaitis and Dennis M. Akos
// Based on code by DMAkos Oct-1999
// Updated and converted to scilab 5.3.0 by Artyom Gavrilov
//--------------------------------------------------------------------------
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either version 2
//of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
//USA.
//--------------------------------------------------------------------------

// Initialize result structure ============================================

// Channel status
trackResults.status         = '-';      // No tracked signal, or lost lock

// The absolute sample in the record of the C/A code start:
trackResults.absoluteSample = zeros(1, settings.msToProcess);

// Freq of the C/A code:
trackResults.codeFreq       = ones(1, settings.msToProcess).*%inf;

// Frequency of the tracked carrier wave:
trackResults.carrFreq       = ones(1, settings.msToProcess).*%inf;

// Outputs from the correlators (In-phase):
trackResults.I_P            = zeros(1, settings.msToProcess);
trackResults.I_E            = zeros(1, settings.msToProcess);
trackResults.I_L            = zeros(1, settings.msToProcess);

// Outputs from the correlators (Quadrature-phase):
trackResults.Q_E            = zeros(1, settings.msToProcess);
trackResults.Q_P            = zeros(1, settings.msToProcess);
trackResults.Q_L            = zeros(1, settings.msToProcess);

// Loop discriminators
trackResults.dllDiscr       = ones(1, settings.msToProcess).*%inf;
trackResults.dllDiscrFilt   = ones(1, settings.msToProcess).*%inf;
trackResults.pllDiscr       = ones(1, settings.msToProcess).*%inf;
trackResults.pllDiscrFilt   = ones(1, settings.msToProcess).*%inf;

//--- Copy initial settings for all channels -------------------------------
if settings.numberOfChannels > 0 then
  trackResults_tmp = trackResults;
  for i=2:settings.numberOfChannels
    trackResults_tmp = [trackResults_tmp trackResults];
  end;
  trackResults = trackResults_tmp;
  clear trackResults_tmp;  
else
  clear trackResults;
end;

// Initialize tracking variables ==========================================

codePeriods = settings.msToProcess;     // For GLONASS one CT code is one ms

//--- DLL variables --------------------------------------------------------
// Define early-late offset (in chips)
earlyLateSpc = settings.dllCorrelatorSpacing;

// Summation interval
PDIcode = 0.001;

// Calculate filter coefficient values
[tau1code, tau2code] = calcLoopCoef(settings.dllNoiseBandwidth, ...
                                    settings.dllDampingRatio, ...
                                    1.0);

//--- PLL variables --------------------------------------------------------
// Summation interval
PDIcarr = 0.001;

// Calculate filter coefficient values
[tau1carr, tau2carr] = calcLoopCoef(settings.pllNoiseBandwidth, ...
                                    settings.pllDampingRatio, ...
                                    0.25);
hwb = waitbar(0,'Tracking...');

if (settings.fileType==1)
    dataAdaptCoeff=1;
else
    dataAdaptCoeff=2;
end

// Start processing channels ==============================================
for channelNr = 1:settings.numberOfChannels
    
    // Only process if PRN is non zero (acquisition was successful)
    if (channel(channelNr).PRN ~= 0)
        // Save additional information - each channel's tracked PRN
        trackResults(channelNr).PRN     = channel(channelNr).PRN;
        
        // Move the starting point of processing. Can be used to start the
        // signal processing at any point in the data record (e.g. for long
        // records). In addition skip through that data file to start at the
        // appropriate sample (corresponding to code phase). Assumes sample
        // type is schar (or 1 byte per sample) 
        mseek(dataAdaptCoeff*(settings.skipNumberOfBytes + channel(channelNr).codePhase-1), fid);

        // Get a vector with the C/A code sampled 1x/chip
        caCode = generateCAcode(101);
        // Then make it possible to do early and late versions
        caCode = [caCode(511) caCode caCode(1)];

        //--- Perform various initializations ------------------------------

        // define initial code frequency basis of NCO
        codeFreq      = settings.codeFreqBasis;
        // define residual code phase (in chips)
        remCodePhase  = 0.0;
        // define carrier frequency which is used over whole tracking period
        carrFreq      = channel(channelNr).acquiredFreq;
        carrFreqBasis = channel(channelNr).acquiredFreq;
        // define residual carrier phase
        remCarrPhase  = 0.0;

        //code tracking loop parameters
        oldCodeNco   = 0.0;
        oldCodeError = 0.0;

        //carrier/Costas loop parameters
        oldCarrNco   = 0.0;
        oldCarrError = 0.0;
        
        //temp variables! We have to use them in order to speed up the code!
        //Structs are extemly slow in scilab 5.3.0 :(
        loopCnt_carrFreq       = ones(1,  settings.msToProcess);
        loopCnt_codeFreq       = ones(1,  settings.msToProcess);
        loopCnt_absoluteSample = zeros(1, settings.msToProcess);
        loopCnt_dllDiscr       = ones(1,  settings.msToProcess);
        loopCnt_dllDiscrFilt   = ones(1,  settings.msToProcess);
        loopCnt_pllDiscr       = ones(1,  settings.msToProcess);
        loopCnt_pllDiscrFilt   = ones(1,  settings.msToProcess);
        loopCnt_I_E            = zeros(1, settings.msToProcess);
        loopCnt_I_P            = zeros(1, settings.msToProcess);
        loopCnt_I_L            = zeros(1, settings.msToProcess);
        loopCnt_Q_E            = zeros(1, settings.msToProcess);
        loopCnt_Q_P            = zeros(1, settings.msToProcess);
        loopCnt_Q_L            = zeros(1, settings.msToProcess);
        
        loopCnt_samplingFreq     = settings.samplingFreq;
        loopCnt_codeLength       = settings.codeLength;
        loopCnt_dataType         = settings.dataType;
        loopCnt_codeFreqBasis    = settings.codeFreqBasis;
        loopCnt_numberOfChannels = settings.numberOfChannels

        //=== Process the number of specified code periods =================
        
        //======================Add file writing routines===================
//        [fd01, err01] = mopen('e:\signal_I.bin', 'wb');
//        [fd02, err02] = mopen('e:\signal_Q.bin', 'wb');
//        [fd03, err03] = mopen(strcat(['e:\carrier_I_svn', string(trackResults(channelNr).PRN), '.bin']), 'wb');
//        [fd04, err04] = mopen(strcat(['e:\carrier_Q_svn', string(trackResults(channelNr).PRN), '.bin']), 'wb');
//        [fd05, err05] = mopen(strcat(['e:\code_svn', string(trackResults(channelNr).PRN), '.bin']), 'wb');
        
        for loopCnt =  1:codePeriods
            
// GUI update -------------------------------------------------------------
            // The GUI is updated every 50ms.
            if (  (loopCnt-fix(loopCnt/50)*50) == 0  )
//Should be corrected in future! Doesn't work like original version :(
                try
                  wbrMsg = strcat(['Tracking: Ch ' string(channelNr) ' of ' ...
                                  string(loopCnt_numberOfChannels) '; PRN#' ...
                                  string(channel(channelNr).PRN)]);
                  waitbar(loopCnt/codePeriods, wbrMsg, hwb); 
                catch
                    // The progress bar was closed. It is used as a signal
                    // to stop, "cancel" processing. Exit.
                    disp('Progress bar closed, exiting...');
                    return
                end
            end

// Read next block of data ------------------------------------------------            
            // Find the size of a "block" or code period in whole samples
            
            // Update the phasestep based on code freq (variable) and
            // sampling frequency (fixed)
            codePhaseStep = codeFreq / loopCnt_samplingFreq;
            
            blksize = ceil((loopCnt_codeLength-remCodePhase) / codePhaseStep);
            
            // Read in the appropriate number of samples to process this
            // interation 
            rawSignal = mget(dataAdaptCoeff*blksize, loopCnt_dataType, fid);
            samplesRead = length(rawSignal);
 
            if (dataAdaptCoeff==2)
                rawSignal1 = rawSignal(1:2:$);
                rawSignal2 = rawSignal(2:2:$);
                rawSignal = rawSignal1 + %i .* rawSignal2;  //transpose vector
            end
            
            
            // If did not read in enough samples, then could be out of 
            // data - better exit 
            if (samplesRead ~= dataAdaptCoeff*blksize)
                disp('Not able to read the specified number of samples  for tracking, exiting!')
                mclose(fid);
                return
            end

// Set up all the code phase tracking information -------------------------
            // Define index into early code vector
            tcode       = (remCodePhase-earlyLateSpc) : ...
                          codePhaseStep : ...
                          ((blksize-1)*codePhaseStep+remCodePhase-earlyLateSpc);
            tcode2      = ceil(tcode) + 1;
            earlyCode   = caCode(tcode2);
            
            // Define index into late code vector
            tcode       = (remCodePhase+earlyLateSpc) : ...
                          codePhaseStep : ...
                          ((blksize-1)*codePhaseStep+remCodePhase+earlyLateSpc);
            tcode2      = ceil(tcode) + 1;
            lateCode    = caCode(tcode2);

            // Define index into prompt code vector
            tcode       = remCodePhase : ...
                          codePhaseStep : ...
                          ((blksize-1)*codePhaseStep+remCodePhase);
            tcode2      = ceil(tcode) + 1;
            promptCode  = caCode(tcode2);
            
            remCodePhase = (tcode(blksize) + codePhaseStep) - 511.0;

// Generate the carrier frequency to mix the signal to baseband -----------
            time    = (0:blksize) ./ loopCnt_samplingFreq;
            
            // Get the argument to sin/cos functions
            trigarg = ((carrFreq * 2.0 * %pi) .* time) + remCarrPhase;
            remCarrPhase = trigarg(blksize+1)-fix(trigarg(blksize+1)./(2 * %pi)).*(2 * %pi);
            
            // Finally compute the signal to mix the collected data to
            // bandband
            carrsig = exp(%i .* trigarg(1:blksize));
            
            
            //===============File writing utils===============================
//            mput(real(rawSignal), 'd', fd01);
//            mput(imag(rawSignal), 'd', fd02);
//            mput(real(carrsig), 'd', fd03);
//            mput(imag(carrsig), 'd', fd04);
//            mput(promptCode, 'd', fd05);
            

// Generate the six standard accumulated values ---------------------------
            // First mix to baseband
            qBasebandSignal = real(carrsig .* rawSignal);
            iBasebandSignal = imag(carrsig .* rawSignal);

            // Now get early, late, and prompt values for each
            I_E = sum(earlyCode  .* iBasebandSignal);
            Q_E = sum(earlyCode  .* qBasebandSignal);
            I_P = sum(promptCode .* iBasebandSignal);
            Q_P = sum(promptCode .* qBasebandSignal);
            I_L = sum(lateCode   .* iBasebandSignal);
            Q_L = sum(lateCode   .* qBasebandSignal);
            
// Find PLL error and update carrier NCO ----------------------------------

            // Implement carrier loop discriminator (phase detector)
            carrError = atan(Q_P / I_P) / (2.0 * %pi);
            
            // Implement carrier loop filter and generate NCO command
            carrNco = oldCarrNco + (tau2carr/tau1carr) * ...
                (carrError - oldCarrError) + carrError * (PDIcarr/tau1carr);
            oldCarrNco   = carrNco;
            oldCarrError = carrError;
            
            oldCarrNco = carrNco;
            oldCarrError = carrError;

            // Modify carrier freq based on NCO command
            carrFreq = carrFreqBasis + carrNco;

            loopCnt_carrFreq(loopCnt) = carrFreq;

// Find DLL error and update code NCO -------------------------------------
            codeError = (sqrt(I_E * I_E + Q_E * Q_E) -...
                         sqrt(I_L * I_L + Q_L * Q_L)) / ...
                        (sqrt(I_E * I_E + Q_E * Q_E) +...
                         sqrt(I_L * I_L + Q_L * Q_L));
            
            // Implement code loop filter and generate NCO command
            codeNco = oldCodeNco + (tau2code/tau1code) * ...
                (codeError - oldCodeError) + codeError * (PDIcode/tau1code);
            oldCodeNco   = codeNco;
            oldCodeError = codeError;
            
            // Modify code freq based on NCO command
            codeFreq = loopCnt_codeFreqBasis - codeNco;
            
            loopCnt_codeFreq(loopCnt) = codeFreq;

// Record various measures to show in postprocessing ----------------------
            // Record sample number (based on 8bit samples)
            loopCnt_absoluteSample(loopCnt) =(mtell(fid))/dataAdaptCoeff;

            loopCnt_dllDiscr(loopCnt)       = codeError;
            loopCnt_dllDiscrFilt(loopCnt)   = codeNco;
            loopCnt_pllDiscr(loopCnt)       = carrError;
            loopCnt_pllDiscrFilt(loopCnt)   = carrNco;

            loopCnt_I_E(loopCnt) = I_E;
            loopCnt_I_P(loopCnt) = I_P;
            loopCnt_I_L(loopCnt) = I_L;
            loopCnt_Q_E(loopCnt) = Q_E;
            loopCnt_Q_P(loopCnt) = Q_P;
            loopCnt_Q_L(loopCnt) = Q_L;
        end // for loopCnt

        // If we got so far, this means that the tracking was successful
        // Now we only copy status, but it can be update by a lock detector
        // if implemented
        trackResults(channelNr).status  = channel(channelNr).status;
        
        //Now copy all data from temp variable to the real place! 
        //We do it to speed up the code.
        trackResults(channelNr).carrFreq       = loopCnt_carrFreq;
        trackResults(channelNr).codeFreq       = loopCnt_codeFreq;
        trackResults(channelNr).absoluteSample = loopCnt_absoluteSample;
        trackResults(channelNr).dllDiscr       = loopCnt_dllDiscr;
        trackResults(channelNr).dllDiscrFilt   = loopCnt_dllDiscrFilt;
        trackResults(channelNr).pllDiscr       = loopCnt_pllDiscr;
        trackResults(channelNr).pllDiscrFilt   = loopCnt_pllDiscrFilt;
        trackResults(channelNr).I_E            = loopCnt_I_E;
        trackResults(channelNr).I_P            = loopCnt_I_P;
        trackResults(channelNr).I_L            = loopCnt_I_L;
        trackResults(channelNr).Q_E            = loopCnt_Q_E;
        trackResults(channelNr).Q_P            = loopCnt_Q_P;
        trackResults(channelNr).Q_L            = loopCnt_Q_L;
        
        
    end // if a PRN is assigned
    
    //======================Add file writing routines===================
//    mclose(fd01);
//    mclose(fd02);
//    mclose(fd03);
//    mclose(fd04);
//    mclose(fd05);
    
end // for channelNr 

// Close the waitbar
winclose(hwb)

endfunction
