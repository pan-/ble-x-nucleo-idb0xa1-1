/* mbed Microcontroller Library
* Copyright (c) 2006-2013 ARM Limited
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/


/**
  ******************************************************************************
  * @file    BlueNRGGap.cpp 
  * @author  STMicroelectronics
  * @brief   Implementation of BLE_API Gap Class
  ******************************************************************************
  * @copy
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2013 STMicroelectronics</center></h2>
  */ 

// ANDREA: Changed some types (e.g., tHalUint8 --> uint8_t)

/** @defgroup BlueNRGGap
 *  @brief BlueNRG BLE_API GAP Adaptation
 *  @{
 */

#include "BlueNRGDevice.h"
#include "mbed-drivers/mbed.h"
#include "Payload.h"
#include "Utils.h"
#include "debug.h"

//Local Variables
//const char *local_name = NULL;
//uint8_t local_name_length = 0;
const uint8_t *scan_response_payload = NULL;
uint8_t scan_rsp_length = 0;

uint32_t advtInterval = BLUENRG_GAP_ADV_INTERVAL_MAX;

/*
 * Utility to process GAP specific events (e.g., Advertising timeout)
 */
void BlueNRGGap::Process(void)
{    
    if(AdvToFlag) {
        stopAdvertising();
    }

}

/**************************************************************************/
/*!
    @brief  Sets the advertising parameters and payload for the device. 
            Note: Some data types give error when their adv data is updated using aci_gap_update_adv_data() API

    @params[in] advData
                The primary advertising data payload
    @params[in] scanResponse
                The optional Scan Response payload if the advertising
                type is set to \ref GapAdvertisingParams::ADV_SCANNABLE_UNDIRECTED
                in \ref GapAdveritinngParams

    @returns    \ref ble_error_t

    @retval     BLE_ERROR_NONE
                Everything executed properly

    @retval     BLE_ERROR_BUFFER_OVERFLOW
                The proposed action would cause a buffer overflow.  All
                advertising payloads must be <= 31 bytes, for example.

    @retval     BLE_ERROR_NOT_IMPLEMENTED
                A feature was requested that is not yet supported in the
                nRF51 firmware or hardware.

    @retval     BLE_ERROR_PARAM_OUT_OF_RANGE
                One of the proposed values is outside the valid range.

    @section EXAMPLE

    @code

    @endcode
*/
/**************************************************************************/
ble_error_t BlueNRGGap::setAdvertisingData(const GapAdvertisingData &advData, const GapAdvertisingData &scanResponse)
{ 
    PRINTF("BlueNRGGap::setAdvertisingData\n\r");
    /* Make sure we don't exceed the advertising payload length */
    if (advData.getPayloadLen() > GAP_ADVERTISING_DATA_MAX_PAYLOAD) {
        return BLE_ERROR_BUFFER_OVERFLOW;
    }

    /* Make sure we have a payload! */
    if (advData.getPayloadLen() <= 0) {
        return BLE_ERROR_PARAM_OUT_OF_RANGE;
    } else { 
        PayloadPtr loadPtr(advData.getPayload(), advData.getPayloadLen());        
        for(uint8_t index=0; index<loadPtr.getPayloadUnitCount(); index++) {                  
            loadPtr.getUnitAtIndex(index);

            PRINTF("adData[%d].length=%d\n\r", index,(uint8_t)(*loadPtr.getUnitAtIndex(index).getLenPtr()));
            PRINTF("adData[%d].AdType=0x%x\n\r", index,(uint8_t)(*loadPtr.getUnitAtIndex(index).getAdTypePtr()));                  
            
            switch(*loadPtr.getUnitAtIndex(index).getAdTypePtr()) {
            case GapAdvertisingData::FLAGS:                              /* ref *Flags */                     
                {
                PRINTF("Advertising type: FLAGS\n\r");
                //Check if Flags are OK. BlueNRG only supports LE Mode.
                uint8_t *flags = loadPtr.getUnitAtIndex(index).getDataPtr();
                if((*flags & GapAdvertisingData::BREDR_NOT_SUPPORTED) != GapAdvertisingData::BREDR_NOT_SUPPORTED) {
                    PRINTF("BlueNRG does not support BR/EDR Mode");
                    return BLE_ERROR_PARAM_OUT_OF_RANGE;
                }
                
                break;
                }
            case GapAdvertisingData::INCOMPLETE_LIST_16BIT_SERVICE_IDS:  /**< Incomplete list of 16-bit Service IDs */
            case GapAdvertisingData::COMPLETE_LIST_16BIT_SERVICE_IDS:    /**< Complete list of 16-bit Service IDs */
            case GapAdvertisingData::INCOMPLETE_LIST_128BIT_SERVICE_IDS: /**< Incomplete list of 128-bit Service IDs */
            case GapAdvertisingData::COMPLETE_LIST_128BIT_SERVICE_IDS:   /**< Complete list of 128-bit Service IDs */
                {
                PRINTF("Advertising type: INCOMPLETE_LIST SERVICE_IDS/COMPLETE_LIST SERVICE_IDS\n\r");
                
                uint8_t buffSize = *loadPtr.getUnitAtIndex(index).getLenPtr()-1;
                // The total lenght should include the Data Type Value
                if(buffSize>UUID_BUFFER_SIZE-1) {
                    return BLE_ERROR_INVALID_PARAM;
                }
                
                servUuidlength = buffSize+1; // +1 to include the Data Type Value
                servUuidData[0] = (uint8_t)(*loadPtr.getUnitAtIndex(index).getAdTypePtr()); //Data Type Value
                
                PRINTF("servUuidlength=%d servUuidData[0]=%d buffSize=%d\n\r", servUuidlength, servUuidData[0], buffSize);
                // Save the Service UUID list just after the Data Type Value field
                memcpy(servUuidData+1, loadPtr.getUnitAtIndex(index).getDataPtr(), buffSize);
#ifdef DEBUG
                for(unsigned i=0; i<servUuidlength; i++) {
                    PRINTF("servUuidData[%d] = 0x%x\n\r", i, servUuidData[i]);
                }
                
                for(unsigned i=0; i<buffSize; i++) {
                    PRINTF("loadPtr.getUnitAtIndex(index).getDataPtr()[%d] = 0x%x\n\r", i, loadPtr.getUnitAtIndex(index).getDataPtr()[i]);
                }
#endif /* DEBUG */
                break;
                }
            case GapAdvertisingData::INCOMPLETE_LIST_32BIT_SERVICE_IDS:  /**< Incomplete list of 32-bit Service IDs (not relevant for Bluetooth 4.0) */
                {
                PRINTF("Advertising type: INCOMPLETE_LIST_32BIT_SERVICE_IDS\n\r");
                return BLE_ERROR_NOT_IMPLEMENTED;
                }
            case GapAdvertisingData::COMPLETE_LIST_32BIT_SERVICE_IDS:    /**< Complete list of 32-bit Service IDs (not relevant for Bluetooth 4.0) */
                {
                PRINTF("Advertising type: COMPLETE_LIST_32BIT_SERVICE_IDS\n\r");
                return BLE_ERROR_NOT_IMPLEMENTED;
                }
            case GapAdvertisingData::SHORTENED_LOCAL_NAME:               /**< Shortened Local Name */
                {
                break;
                }
            case GapAdvertisingData::COMPLETE_LOCAL_NAME:                /**< Complete Local Name */
                {
                PRINTF("Advertising type: COMPLETE_LOCAL_NAME\n\r");
                loadPtr.getUnitAtIndex(index).printDataAsString();
                local_name_length = *loadPtr.getUnitAtIndex(index).getLenPtr()-1;
                // The total lenght should include the Data Type Value
                if(local_name_length>LOCAL_NAME_MAX_SIZE-1) {
                    return BLE_ERROR_INVALID_PARAM;
                }
                local_name[0] = (uint8_t)(*loadPtr.getUnitAtIndex(index).getAdTypePtr()); //Data Type Value
                memcpy(local_name+1, (uint8_t*)loadPtr.getUnitAtIndex(index).getDataPtr(), local_name_length-1);
                PRINTF("Advertising type: COMPLETE_LOCAL_NAME local_name=%s local_name_length=%d\n\r", local_name, local_name_length);

                break;
                }
            case GapAdvertisingData::TX_POWER_LEVEL:                     /**< TX Power Level (in dBm) */
                {
                PRINTF("Advertising type: TX_POWER_LEVEL\n\r");     
                int8_t enHighPower = 0;
                int8_t paLevel = 0;
#ifdef DEBUG
                int8_t dbm = *loadPtr.getUnitAtIndex(index).getDataPtr();
                int8_t dbmActuallySet = getHighPowerAndPALevelValue(dbm, enHighPower, paLevel);
#endif
                PRINTF("dbm=%d, dbmActuallySet=%d\n\r", dbm, dbmActuallySet);
                PRINTF("enHighPower=%d, paLevel=%d\n\r", enHighPower, paLevel);                    
                aci_hal_set_tx_power_level(enHighPower, paLevel);
                break;
                }
            case GapAdvertisingData::DEVICE_ID:                          /**< Device ID */
                {
                break;
                }
            case GapAdvertisingData::SLAVE_CONNECTION_INTERVAL_RANGE:    /**< Slave :Connection Interval Range */
                {
                break;
                }
            case GapAdvertisingData::SERVICE_DATA:                       /**< Service Data */
                {
                PRINTF("Advertising type: SERVICE_DATA\n\r");
                uint8_t buffSize = *loadPtr.getUnitAtIndex(index).getLenPtr()-1;
                PRINTF("Advertising type: SERVICE_DATA (buffSize=%d)\n\r", buffSize);
                // the total ADV DATA LEN should include two more bytes: the buffer size byte; and the Service Data Type Value byte
                if(buffSize>ADV_DATA_MAX_SIZE-2) {
                    return BLE_ERROR_PARAM_OUT_OF_RANGE;
                }
                for(int i=0; i<buffSize+1; i++) {
                    PRINTF("Advertising type: SERVICE_DATA loadPtr.getUnitAtIndex(index).getDataPtr()[%d] = 0x%x\n\r", i, loadPtr.getUnitAtIndex(index).getDataPtr()[i]);
                }
                AdvLen = buffSize+2; // the total ADV DATA LEN should include two more bytes: the buffer size byte; and the Service Data Type Value byte
                AdvData[0] = buffSize+1; // the fisrt byte is the data buffer size (type+data)
                AdvData[1] = AD_TYPE_SERVICE_DATA;
                memcpy(AdvData+2, loadPtr.getUnitAtIndex(index).getDataPtr(), buffSize);
                break;
                }
            case GapAdvertisingData::APPEARANCE:			/**< Appearance */
                {
                PRINTF("Advertising type: APPEARANCE\n\r");
                uint8_t buffSize = *loadPtr.getUnitAtIndex(index).getLenPtr()-1;
                if(buffSize>ADV_DATA_MAX_SIZE-2) {
                    return BLE_ERROR_PARAM_OUT_OF_RANGE;
                }
                GapAdvertisingData::Appearance appearanceP;
                memcpy(deviceAppearance, loadPtr.getUnitAtIndex(index).getDataPtr(), 2);
                
                PRINTF("input: deviceAppearance= 0x%x 0x%x\n\r", deviceAppearance[1], deviceAppearance[0]);

                appearanceP = (GapAdvertisingData::Appearance)(deviceAppearance[1]<<8|deviceAppearance[0]);
                /* Align the GAP Service Appearance Char value coherently */
                setAppearance(appearanceP);
                break;
                }
            case GapAdvertisingData::ADVERTISING_INTERVAL:               /**< Advertising Interval */
                {
                PRINTF("Advertising type: ADVERTISING_INTERVAL\n\r");
                advtInterval = (uint16_t)(*loadPtr.getUnitAtIndex(index).getDataPtr());
                PRINTF("advtInterval=%d\n\r", (int)advtInterval);
                break;
                }
            case GapAdvertisingData::MANUFACTURER_SPECIFIC_DATA:        /**< Manufacturer Specific Data */                             
                {
                PRINTF("Advertising type: MANUFACTURER_SPECIFIC_DATA\n\r");
                uint8_t buffSize = *loadPtr.getUnitAtIndex(index).getLenPtr()-1;
                PRINTF("Advertising type: MANUFACTURER_SPECIFIC_DATA (buffSize=%d)\n\r", buffSize);
                // the total ADV DATA LEN should include two more bytes:
		// the buffer size byte;
		// and the Manufacturer Specific Data Type Value byte
                if(buffSize>ADV_DATA_MAX_SIZE-2) {
                    return BLE_ERROR_PARAM_OUT_OF_RANGE;
                }
                for(int i=0; i<buffSize+1; i++) {
                    PRINTF("Advertising type: MANUFACTURER_SPECIFIC_DATA loadPtr.getUnitAtIndex(index).getDataPtr()[%d] = 0x%x\n\r",
				i, loadPtr.getUnitAtIndex(index).getDataPtr()[i]);
                }
                AdvLen = buffSize+2; // the total ADV DATA LEN should include two more bytes: the buffer size byte; and the Manufacturer Specific Data Type Value byte
                AdvData[0] = buffSize+1; // the fisrt byte is the data buffer size (type+data)
                AdvData[1] = AD_TYPE_MANUFACTURER_SPECIFIC_DATA;
                memcpy(AdvData+2, loadPtr.getUnitAtIndex(index).getDataPtr(), buffSize);
                break;
                }
                
            }          
        }
        //Set the SCAN_RSP Payload
        scan_response_payload = scanResponse.getPayload();
        scan_rsp_length = scanResponse.getPayloadLen();
        
        // Update the ADV data if we are already in ADV mode
        if(AdvLen > 0 && state.advertising == 1) {
 
            tBleStatus ret = aci_gap_update_adv_data(AdvLen, AdvData);
            if(BLE_STATUS_SUCCESS!=ret) {
                PRINTF("error occurred while adding adv data (ret=0x%x)\n", ret);
                switch (ret) {
                    case BLE_STATUS_TIMEOUT:
                        return BLE_STACK_BUSY;
                    default:
                        return BLE_ERROR_UNSPECIFIED;
                }
            }
        }
    }
    return BLE_ERROR_NONE;
}

/*
 * Utility to set ADV timeout flag
 */
void BlueNRGGap::setAdvToFlag(void) {
    AdvToFlag = true;
}

/*
 * ADV timeout callback
 */   
// ANDREA: mbedOS
#ifdef AST_FOR_MBED_OS
static void advTimeoutCB(void)
{
    Gap::GapState_t state;
    
    state = BlueNRGGap::getInstance().getState();
    if (state.advertising == 1) {
        
        BlueNRGGap::getInstance().stopAdvertising();
        
    }
}
#else
static void advTimeoutCB(void)
{
    Gap::GapState_t state;
    
    state = BlueNRGGap::getInstance().getState();
    if (state.advertising == 1) {
        
        BlueNRGGap::getInstance().setAdvToFlag();
        
        Timeout t = BlueNRGGap::getInstance().getAdvTimeout();
        t.detach(); /* disable the callback from the timeout */

    }
}
#endif /* AST_FOR_MBED_OS */
    
/**************************************************************************/
/*!
    @brief  Starts the BLE HW, initialising any services that were
            added before this function was called.
    
    @param[in]  params
                Basic advertising details, including the advertising
                delay, timeout and how the device should be advertised
                
    @note   All services must be added before calling this function!

    @returns    ble_error_t

    @retval     BLE_ERROR_NONE
                Everything executed properly

    @section EXAMPLE

    @code

    @endcode
*/
/**************************************************************************/

ble_error_t BlueNRGGap::startAdvertising(const GapAdvertisingParams &params)
{
    tBleStatus ret;

    /* Make sure we support the advertising type */
    if (params.getAdvertisingType() == GapAdvertisingParams::ADV_CONNECTABLE_DIRECTED) {
        /* ToDo: This requires a proper security implementation, etc. */
        return BLE_ERROR_NOT_IMPLEMENTED;
    }

    /* Check interval range */
    if (params.getAdvertisingType() == GapAdvertisingParams::ADV_NON_CONNECTABLE_UNDIRECTED) {
        /* Min delay is slightly longer for unconnectable devices */
        if ((params.getIntervalInADVUnits() < GapAdvertisingParams::GAP_ADV_PARAMS_INTERVAL_MIN_NONCON) ||
                (params.getIntervalInADVUnits() > GapAdvertisingParams::GAP_ADV_PARAMS_INTERVAL_MAX)) {
            return BLE_ERROR_PARAM_OUT_OF_RANGE;
        }
    } else {
        if ((params.getIntervalInADVUnits() < GapAdvertisingParams::GAP_ADV_PARAMS_INTERVAL_MIN) ||
                (params.getIntervalInADVUnits() > GapAdvertisingParams::GAP_ADV_PARAMS_INTERVAL_MAX)) {
            return BLE_ERROR_PARAM_OUT_OF_RANGE;
        }
    }

    /* Check timeout is zero for Connectable Directed */
    if ((params.getAdvertisingType() == GapAdvertisingParams::ADV_CONNECTABLE_DIRECTED) && (params.getTimeout() != 0)) {
        /* Timeout must be 0 with this type, although we'll never get here */
        /* since this isn't implemented yet anyway */
        return BLE_ERROR_PARAM_OUT_OF_RANGE;
    }

    /* Check timeout for other advertising types */
    if ((params.getAdvertisingType() != GapAdvertisingParams::ADV_CONNECTABLE_DIRECTED) &&
            (params.getTimeout() > GapAdvertisingParams::GAP_ADV_PARAMS_TIMEOUT_MAX)) {
        return BLE_ERROR_PARAM_OUT_OF_RANGE;
    }

    /* Check the ADV type before setting scan response data */
    if (params.getAdvertisingType() == GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED ||
        params.getAdvertisingType() == GapAdvertisingParams::ADV_SCANNABLE_UNDIRECTED) {

        /* set scan response data */
        PRINTF(" setting scan response data (scan_rsp_length=%u)\n", scan_rsp_length);
        ret = hci_le_set_scan_resp_data(scan_rsp_length, scan_response_payload);

        if(BLE_STATUS_SUCCESS!=ret) {
            PRINTF(" error while setting scan response data (ret=0x%x)\n", ret);
            switch (ret) {
              case BLE_STATUS_TIMEOUT:
                return BLE_STACK_BUSY;
              default:
                return BLE_ERROR_UNSPECIFIED;
            }
        }
    } else {
        hci_le_set_scan_resp_data(0, NULL);
    }

    advtInterval = params.getIntervalInADVUnits();
    printf("advtInterval=%ld advType=%d\n\r", advtInterval, params.getAdvertisingType());

    ret = aci_gap_set_discoverable(params.getAdvertisingType(), // AdvType
                                   advtInterval,                // AdvIntervMin
                                   advtInterval,                // AdvIntervMax
                                   addr_type,                   // OwnAddrType
                                   NO_WHITE_LIST_USE,           // AdvFilterPolicy
                                   local_name_length,           // LocalNameLen
                                   (const char*)local_name,     // LocalName
                                   servUuidlength,              // ServiceUUIDLen
                                   servUuidData,                // ServiceUUIDList
                                   0,                           // SlaveConnIntervMin
                                   0);                          // SlaveConnIntervMax

    
    printf("!!!setting discoverable (servUuidlength=0x%x)\n\r", servUuidlength);
    if(BLE_STATUS_SUCCESS!=ret) {
       printf("error occurred while setting discoverable (ret=0x%x)\n\r", ret);
       switch (ret) {
         case BLE_STATUS_INVALID_PARAMS:
           return BLE_ERROR_INVALID_PARAM;
         case ERR_COMMAND_DISALLOWED:
           return BLE_ERROR_OPERATION_NOT_PERMITTED;
         case ERR_UNSUPPORTED_FEATURE:
           return BLE_ERROR_NOT_IMPLEMENTED;
         case BLE_STATUS_TIMEOUT:
           return BLE_STACK_BUSY;
         default:
           return BLE_ERROR_UNSPECIFIED;
       }
    }

    // Before updating the ADV data, delete COMPLETE_LOCAL_NAME and TX_POWER_LEVEL fields (if present)
    if(AdvLen > 0) {
      if(local_name_length > 0) {
        PRINTF("!!!calling aci_gap_delete_ad_type AD_TYPE_COMPLETE_LOCAL_NAME!!!\n");
        ret = aci_gap_delete_ad_type(AD_TYPE_COMPLETE_LOCAL_NAME);
        if (BLE_STATUS_SUCCESS!=ret){
          PRINTF("aci_gap_delete_ad_type failed return=%d\n", ret);
          switch (ret) {
            case BLE_STATUS_TIMEOUT:
              return BLE_STACK_BUSY;
            default:
              return BLE_ERROR_UNSPECIFIED;
          }
        }
      }

      // If ADV Data Type is SERVICE DATA or MANUFACTURER SPECIFIC DATA,
      // we need to delete it to make the needed room in ADV payload
      if(AdvData[1]==AD_TYPE_SERVICE_DATA || AdvData[1]==AD_TYPE_MANUFACTURER_SPECIFIC_DATA) {
        PRINTF("!!!calling aci_gap_delete_ad_type(AD_TYPE_TX_POWER_LEVEL)!!!\n");
        ret = aci_gap_delete_ad_type(AD_TYPE_TX_POWER_LEVEL);
        if (BLE_STATUS_SUCCESS!=ret){
          PRINTF("aci_gap_delete_ad_type failed return=%d\n", ret);
          switch (ret) {
            case BLE_STATUS_TIMEOUT:
              return BLE_STACK_BUSY;
            default:
              return BLE_ERROR_UNSPECIFIED;
          }
        }
      }
   
      ret = aci_gap_update_adv_data(AdvLen, AdvData);
      if(BLE_STATUS_SUCCESS!=ret) {
        PRINTF("error occurred while adding adv data (ret=0x%x)\n", ret);
          switch (ret) {
            case BLE_STATUS_TIMEOUT:
              return BLE_STACK_BUSY;
            default:
              return BLE_ERROR_UNSPECIFIED;
          }
      }

    } // AdvLen>0

    if(deviceAppearance != 0) {
      PRINTF("deviceAppearance != 0\n\r");
      uint8_t appearance[] = {3, AD_TYPE_APPEARANCE, deviceAppearance[0], deviceAppearance[1]};
      aci_gap_update_adv_data(4, appearance);
    }

    state.advertising = 1;

    AdvToFlag = false;
    if(params.getTimeout() != 0) {
        PRINTF("!!! attaching to!!!\n");
        // ANDREA: mbedOS
#ifdef AST_FOR_MBED_OS
        minar::Scheduler::postCallback(advTimeoutCB).delay(minar::milliseconds(params.getTimeout()));
#else
        advTimeout.attach(advTimeoutCB, params.getTimeout());
#endif
    }
    
    return BLE_ERROR_NONE;
}

/**************************************************************************/
/*!
    @brief  Stops the BLE HW and disconnects from any devices

    @returns    ble_error_t

    @retval     BLE_ERROR_NONE
                Everything executed properly

    @section EXAMPLE

    @code

    @endcode
*/
/**************************************************************************/
ble_error_t BlueNRGGap::stopAdvertising(void)
{
    tBleStatus ret;
    
    if(state.advertising == 1) {
        //Set non-discoverable to stop advertising
        ret = aci_gap_set_non_discoverable();
        
        if (BLE_STATUS_SUCCESS!=ret){
            PRINTF("Error in stopping advertisement (ret=0x%x)!!\n\r", ret) ;
            switch (ret) {
              case ERR_COMMAND_DISALLOWED:
                return BLE_ERROR_OPERATION_NOT_PERMITTED;
              case BLE_STATUS_TIMEOUT:
                return BLE_STACK_BUSY;
              default:
                return BLE_ERROR_UNSPECIFIED;
            }
        }
        PRINTF("Advertisement stopped!!\n\r") ;
        //Set GapState_t::advertising state
        state.advertising = 0;
    }
    
    return BLE_ERROR_NONE;
}

/**************************************************************************/
/*!
    @brief  Disconnects if we are connected to a central device

    @param[in]  reason
                Disconnection Reason
                
    @returns    ble_error_t

    @retval     BLE_ERROR_NONE
                Everything executed properly

    @section EXAMPLE

    @code

    @endcode
*/
/**************************************************************************/
ble_error_t BlueNRGGap::disconnect(Gap::DisconnectionReason_t reason)
{
    /* avoid compiler warnings about unused variables */
    (void)reason;

    tBleStatus ret;
    //For Reason codes check BlueTooth HCI Spec
    
    if(m_connectionHandle != BLE_CONN_HANDLE_INVALID) {
        ret = aci_gap_terminate(m_connectionHandle, 0x16);//0x16 Connection Terminated by Local Host. 

        if (BLE_STATUS_SUCCESS != ret){
            PRINTF("Error in GAP termination (ret=0x%x)!!\n\r", ret) ;
            switch (ret) {
              case ERR_COMMAND_DISALLOWED:
                return BLE_ERROR_OPERATION_NOT_PERMITTED;
              case BLE_STATUS_TIMEOUT:
                return BLE_STACK_BUSY;
              default:
                return BLE_ERROR_UNSPECIFIED;
            }
        }
        
        //PRINTF("Disconnected from localhost!!\n\r") ;
        m_connectionHandle = BLE_CONN_HANDLE_INVALID;
    }
    
    return BLE_ERROR_NONE;
}

/**************************************************************************/
/*!
    @brief  Disconnects if we are connected to a central device

    @param[in]  reason
                Disconnection Reason
                
    @returns    ble_error_t

    @retval     BLE_ERROR_NONE
                Everything executed properly

    @section EXAMPLE

    @code

    @endcode
*/
/**************************************************************************/
ble_error_t BlueNRGGap::disconnect(Handle_t connectionHandle, Gap::DisconnectionReason_t reason)
{
    /* avoid compiler warnings about unused variables */
    (void)reason;

    tBleStatus ret;
    //For Reason codes check BlueTooth HCI Spec
    
    if(connectionHandle != BLE_CONN_HANDLE_INVALID) {
        ret = aci_gap_terminate(connectionHandle, 0x16);//0x16 Connection Terminated by Local Host. 

        if (BLE_STATUS_SUCCESS != ret){
            PRINTF("Error in GAP termination (ret=0x%x)!!\n\r", ret) ;
            switch (ret) {
              case ERR_COMMAND_DISALLOWED:
                return BLE_ERROR_OPERATION_NOT_PERMITTED;
              case BLE_STATUS_TIMEOUT:
                return BLE_STACK_BUSY;
              default:
                return BLE_ERROR_UNSPECIFIED;
            }
        }
        
        //PRINTF("Disconnected from localhost!!\n\r") ;
        m_connectionHandle = BLE_CONN_HANDLE_INVALID;
    }
    
    return BLE_ERROR_NONE;
}

/**************************************************************************/
/*!
    @brief  Sets the 16-bit connection handle
    
    @param[in]  con_handle
                Connection Handle which is set in the Gap Instance
                
    @returns    void
*/
/**************************************************************************/
void BlueNRGGap::setConnectionHandle(uint16_t con_handle)
{
    m_connectionHandle = con_handle;
}

/**************************************************************************/
/*!
    @brief  Gets the 16-bit connection handle
    
    @param[in]  void
                
    @returns    uint16_t
                Connection Handle of the Gap Instance
*/
/**************************************************************************/
uint16_t BlueNRGGap::getConnectionHandle(void)
{
    return m_connectionHandle;
}

/**************************************************************************/
/*!
    @brief      Sets the BLE device address. SetAddress will reset the BLE
                device and re-initialize BTLE. Will not start advertising.

    @param[in]  type
                Type of Address
    
    @param[in]  address[6]
                Value of the Address to be set
                
    @returns    ble_error_t

    @section EXAMPLE

    @code

    @endcode
*/
/**************************************************************************/
/*
ble_error_t BlueNRGGap::setAddress(AddressType_t type, const Address_t address)
{
    tBleStatus ret;

    if (type > BLEProtocol::AddressType::RANDOM_PRIVATE_NON_RESOLVABLE) {
        return BLE_ERROR_PARAM_OUT_OF_RANGE;
    }
    
    addr_type = type;

    // If Address Type is other than PUBLIC, the given Address is ignored
    if(addr_type == BLEProtocol::AddressType::PUBLIC){
        ret = aci_hal_write_config_data(CONFIG_DATA_PUBADDR_OFFSET,
                                        CONFIG_DATA_PUBADDR_LEN,
                                        address);
        printf("setAddress (ret=0x%02X)\n\r", ret);
    }
    
    return BLE_ERROR_NONE;
}
*/
/**************************************************************************/
/*!
    @brief      Returns boolean if the address of the device has been set
                or not
                
    @returns    bool

    @section EXAMPLE

    @code

    @endcode
*/
/**************************************************************************/
bool BlueNRGGap::getIsSetAddress() 
{
    return isSetAddress;   
}

/**************************************************************************/
/*!
    @brief      Returns the address of the device if set

    @returns    Pointer to the address if Address is set else NULL

    @section EXAMPLE

    @code

    @endcode
*/
/**************************************************************************/
ble_error_t BlueNRGGap::getAddress(AddressType_t *typeP, Address_t address) 
{
    if(typeP != NULL) {
        *typeP = addr_type;
    }

    hci_read_bd_addr(bdaddr);
    if(address != NULL) {
        memcpy(address, bdaddr, BDADDR_SIZE);
    }
        
    return BLE_ERROR_NONE;
}

/**************************************************************************/
/*!
    @brief      obtains preferred connection params

    @returns    ble_error_t

    @section EXAMPLE

    @code

    @endcode
*/
/**************************************************************************/
ble_error_t BlueNRGGap::getPreferredConnectionParams(ConnectionParams_t *params) 
{
    /* avoid compiler warnings about unused variables */
    (void)params;

    return BLE_ERROR_NONE;
}


/**************************************************************************/
/*!
    @brief      sets preferred connection params

    @returns    ble_error_t

    @section EXAMPLE

    @code

    @endcode
*/
/**************************************************************************/
ble_error_t BlueNRGGap::setPreferredConnectionParams(const ConnectionParams_t *params) 
{
    /* avoid compiler warnings about unused variables */
    (void)params;

    return BLE_ERROR_NONE;
}

/**************************************************************************/
/*!
    @brief      updates preferred connection params

    @returns    ble_error_t

    @section EXAMPLE

    @code

    @endcode
*/
/**************************************************************************/
ble_error_t BlueNRGGap::updateConnectionParams(Handle_t handle, const ConnectionParams_t *params)
{
    /* avoid compiler warnings about unused variables */
    (void) handle;
    (void)params;

    return BLE_ERROR_NONE;
}

/**************************************************************************/
/*!
    @brief  Sets the Device Name Characteristic 

    @param[in]  deviceName
                pointer to device name to be set

    @returns    ble_error_t

    @retval     BLE_ERROR_NONE
                Everything executed properly

    @section EXAMPLE

    @code

    @endcode
*/
/**************************************************************************/
ble_error_t BlueNRGGap::setDeviceName(const uint8_t *deviceName) 
{
    tBleStatus ret;
    uint8_t nameLen = 0;     
    
    nameLen = strlen((const char*)deviceName);
    PRINTF("DeviceName Size=%d\n\r", nameLen);

    ret = aci_gatt_update_char_value(g_gap_service_handle,
                                     g_device_name_char_handle,
                                     0,
                                     nameLen,
                                     deviceName);

    if (BLE_STATUS_SUCCESS != ret){
        PRINTF("device set name failed (ret=0x%x)!!\n\r", ret) ;
        switch (ret) {
          case BLE_STATUS_INVALID_HANDLE:
          case BLE_STATUS_INVALID_PARAMETER:
            return BLE_ERROR_INVALID_PARAM;
          case BLE_STATUS_INSUFFICIENT_RESOURCES:
            return BLE_ERROR_NO_MEM;
          case BLE_STATUS_TIMEOUT:
            return BLE_STACK_BUSY;
          default:
            return BLE_ERROR_UNSPECIFIED;
        }
    }

    return BLE_ERROR_NONE;
}

/**************************************************************************/
/*!
    @brief  Gets the Device Name Characteristic 

    @param[in]  deviceName
                pointer to device name                 

    @param[in]  lengthP
                pointer to device name length                

    @returns    ble_error_t

    @retval     BLE_ERROR_NONE
                Everything executed properly

    @section EXAMPLE

    @code

    @endcode
*/
/**************************************************************************/
ble_error_t BlueNRGGap::getDeviceName(uint8_t *deviceName, unsigned *lengthP)
{
    tBleStatus ret;

    ret = aci_gatt_read_handle_value(g_device_name_char_handle+CHAR_VALUE_OFFSET,
                                     *lengthP,
                                     (uint16_t *)lengthP,
                                     deviceName);
    PRINTF("getDeviceName ret=0x%02x (lengthP=%d)\n\r", ret, *lengthP);
    if (ret == BLE_STATUS_SUCCESS) {
        return BLE_ERROR_NONE;
    } else {
        return BLE_ERROR_PARAM_OUT_OF_RANGE;
    }
}

/**************************************************************************/
/*!
    @brief  Sets the Device Appearance Characteristic 

    @param[in]  appearance
                device appearance      

    @returns    ble_error_t

    @retval     BLE_ERROR_NONE
                Everything executed properly

    @section EXAMPLE

    @code

    @endcode
*/
/**************************************************************************/
ble_error_t BlueNRGGap::setAppearance(GapAdvertisingData::Appearance appearance)
{
    tBleStatus ret;
    uint8_t deviceAppearance[2];

    STORE_LE_16(deviceAppearance, appearance);                 
    PRINTF("input: incoming = %d deviceAppearance= 0x%x 0x%x\n\r", appearance, deviceAppearance[1], deviceAppearance[0]);
    
    ret = aci_gatt_update_char_value(g_gap_service_handle,
                                     g_appearance_char_handle,
                                     0, 2, (uint8_t *)deviceAppearance);
    if (BLE_STATUS_SUCCESS == ret){
        return BLE_ERROR_NONE;
    }

    PRINTF("setAppearance failed (ret=0x%x)!!\n\r", ret);
    switch (ret) {
      case BLE_STATUS_INVALID_HANDLE:
      case BLE_STATUS_INVALID_PARAMETER:
        return BLE_ERROR_INVALID_PARAM;
      case BLE_STATUS_INSUFFICIENT_RESOURCES:
        return BLE_ERROR_NO_MEM;
      case BLE_STATUS_TIMEOUT:
        return BLE_STACK_BUSY;
      default:
        return BLE_ERROR_UNSPECIFIED;
    }
}

/**************************************************************************/
/*!
    @brief  Gets the Device Appearance Characteristic

    @param[in]  appearance
                pointer to device appearance value      

    @returns    ble_error_t

    @retval     BLE_ERROR_NONE
                Everything executed properly

    @section EXAMPLE

    @code

    @endcode
*/
/**************************************************************************/
ble_error_t BlueNRGGap::getAppearance(GapAdvertisingData::Appearance *appearanceP)
{
    tBleStatus ret;
    uint16_t lengthP = 2;

    ret = aci_gatt_read_handle_value(g_appearance_char_handle+CHAR_VALUE_OFFSET,
                                     lengthP,
                                     &lengthP,
                                     (uint8_t*)appearanceP);
    PRINTF("getAppearance ret=0x%02x (lengthP=%d)\n\r", ret, lengthP);
    if (ret == BLE_STATUS_SUCCESS) {
        return BLE_ERROR_NONE;
    } else {
        return BLE_ERROR_PARAM_OUT_OF_RANGE;
    }

}

GapScanningParams* BlueNRGGap::getScanningParams(void)
{
  return &_scanningParams;
}

static void radioScanning(void)
{
  GapScanningParams* scanningParams = BlueNRGGap::getInstance().getScanningParams();

  BlueNRGGap::getInstance().startRadioScan(*scanningParams);
}

static void makeConnection(void)
{
  BlueNRGGap::getInstance().createConnection();
}

// ANDREA
void BlueNRGGap::Discovery_CB(Reason_t reason,
                              uint8_t adv_type,
                              uint8_t *addr_type,
                              uint8_t *addr,
                              uint8_t *data_length,
                              uint8_t *data,
                              uint8_t *RSSI)
{
  /* avoid compiler warnings about unused variables */
  (void)addr_type;

  switch (reason) {
  case DEVICE_FOUND:
    {
      GapAdvertisingParams::AdvertisingType_t type;
      bool isScanResponse = false;
      switch(adv_type) {
      case ADV_IND:
        type = GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED;
        break;
      case ADV_DIRECT_IND:
        type = GapAdvertisingParams::ADV_CONNECTABLE_DIRECTED;
        break;
      case ADV_SCAN_IND:
      case SCAN_RSP:
        type = GapAdvertisingParams::ADV_SCANNABLE_UNDIRECTED;
        isScanResponse = true;
        break;
      case ADV_NONCONN_IND:
        type = GapAdvertisingParams::ADV_NON_CONNECTABLE_UNDIRECTED;
        break;
      default:
        type = GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED;
      }
    
      PRINTF("data_length=%d adv peerAddr[%02x %02x %02x %02x %02x %02x] \r\n",
             *data_length, addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
      processAdvertisementReport(addr, *RSSI, isScanResponse, type, *data_length, data);
      PRINTF("!!!After processAdvertisementReport\n\r");
    }
    break;
    
  case DISCOVERY_COMPLETE:
    // The discovery is complete. If this is due to a stop scanning (i.e., the device
    // we are interested in has been found) and a connection has been requested
    // then we start the device connection. Otherwise, we restart the scanning.
    PRINTF("DISCOVERY_COMPLETE\n\r");
    _scanning = false;

    // Since the DISCOVERY_COMPLETE event can be received during the scanning interval,
    // we need to delay the starting of connection or re-scanning procedures
    uint16_t delay = 2*(_scanningParams.getInterval());

#ifdef AST_FOR_MBED_OS
    if(_connecting) {
      minar::Scheduler::postCallback(makeConnection).delay(minar::milliseconds(delay));
    } else {
      minar::Scheduler::postCallback(radioScanning).delay(minar::milliseconds(delay));
    }
#else
    Clock_Wait(delay);
    if(_connecting) {
      makeConnection();
    } else {
      radioScanning();
    }
#endif /* AST_FOR_MBED_OS */

    break;
  }
}

ble_error_t BlueNRGGap::startRadioScan(const GapScanningParams &scanningParams)
{
  
  tBleStatus ret = BLE_STATUS_SUCCESS;

  // Stop ADV before scanning
  /*
  if (state.advertising == 1) {
    stopAdvertising();
  }
  */

  PRINTF("Scanning...\n\r");
  ret = btleStartRadioScan(scanningParams.getActiveScanning(),
                           scanningParams.getInterval(),
                           scanningParams.getWindow(),
                           addr_type);


  if (BLE_STATUS_SUCCESS == ret){
    PRINTF("Observation Procedure Started\n");
    _scanning = true;
    return BLE_ERROR_NONE;
  }

  // Observer role is not supported by X-NUCLEO-IDB04A1, return BLE_ERROR_NOT_IMPLEMENTED
  switch (ret) {
    case BLE_STATUS_INVALID_CID:
      printf("Observation Procedure not implemented!!!\n\r");
      return BLE_ERROR_NOT_IMPLEMENTED;
    default:
      printf("Observation Procedure failed (0x%02X)\n\r", ret);
      return BLE_ERROR_UNSPECIFIED;
  }

}

ble_error_t BlueNRGGap::stopScan() {
  tBleStatus ret = BLE_STATUS_SUCCESS;
  
  ret = aci_gap_terminate_gap_procedure(GAP_OBSERVATION_PROC);
  
  if (ret != BLE_STATUS_SUCCESS) {
    printf("GAP Terminate Gap Procedure failed\n");
    return BLE_ERROR_UNSPECIFIED; 
  } else {
    PRINTF("Discovery Procedure Terminated\n");
    return BLE_ERROR_NONE; 
  }
}

/**************************************************************************/
/*!
    @brief  set Tx power level
    @param[in] txPower Transmission Power level
    @returns    ble_error_t
*/
/**************************************************************************/
ble_error_t BlueNRGGap::setTxPower(int8_t txPower)
{
    tBleStatus ret;
    
    int8_t enHighPower = 0;
    int8_t paLevel = 0;

    ret = getHighPowerAndPALevelValue(txPower, enHighPower, paLevel);
    if(ret!=BLE_STATUS_SUCCESS) {
        return BLE_ERROR_PARAM_OUT_OF_RANGE;
    }

    PRINTF("enHighPower=%d, paLevel=%d\n\r", enHighPower, paLevel);                    
    ret = aci_hal_set_tx_power_level(enHighPower, paLevel);
    if(ret!=BLE_STATUS_SUCCESS) {
      return BLE_ERROR_PARAM_OUT_OF_RANGE;
    }

    return BLE_ERROR_NONE;
}

/**************************************************************************/
/*!
    @brief  get permitted Tx power values
    @param[in] values pointer to pointer to permitted power values
    @param[in] num number of values   
*/
/**************************************************************************/
void BlueNRGGap::getPermittedTxPowerValues(const int8_t **valueArrayPP, size_t *countP) {
    static const int8_t permittedTxValues[] = {
        -18, -15, -14, -12, -11, -9, -8, -6, -5 -2, 0, 2, 4, 5, 8
    };

    *valueArrayPP = permittedTxValues;
    *countP = sizeof(permittedTxValues) / sizeof(int8_t);
}

ble_error_t BlueNRGGap::createConnection ()
{
  tBleStatus ret;
  GapScanningParams* scanningParams = getScanningParams();
  
  /*
    Scan_Interval, Scan_Window, Peer_Address_Type, Peer_Address, Own_Address_Type, Conn_Interval_Min, 
    Conn_Interval_Max, Conn_Latency, Supervision_Timeout, Conn_Len_Min, Conn_Len_Max    
  */
  ret = aci_gap_create_connection(scanningParams->getInterval(),
				  scanningParams->getWindow(),
				  PUBLIC_ADDR,
				  (unsigned char*)_peerAddr,
				  PUBLIC_ADDR,
				  CONN_P1, CONN_P2, 0,
				  SUPERV_TIMEOUT, CONN_L1 , CONN_L1);

  _connecting = false;
  
  if (ret != BLE_STATUS_SUCCESS) {
    printf("Error while starting connection (ret=0x%02X).\n\r", ret);
    return BLE_ERROR_UNSPECIFIED;
  } else {
    PRINTF("Connection started.\n");
    return BLE_ERROR_NONE;
  }
}

ble_error_t BlueNRGGap::connect (const Gap::Address_t peerAddr,
                                 Gap::AddressType_t peerAddrType,
                                 const ConnectionParams_t *connectionParams,
                                 const GapScanningParams *scanParams)
{
  /* avoid compiler warnings about unused variables */
  (void)peerAddrType;
  (void)connectionParams;
  (void)scanParams;

  // Save the peer address
  for(int i=0; i<BDADDR_SIZE; i++) {
    _peerAddr[i] = peerAddr[i];
  }

  _connecting = true;

  if(_scanning) {
    stopScan();
  } else {
    PRINTF("Calling createConnection from connect()\n\r");
    return createConnection();
  }
  
  return BLE_ERROR_NONE;
}
