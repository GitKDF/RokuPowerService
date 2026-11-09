/****************************** Module Header ******************************\
* Module Name:  ServiceInstaller.h
* Project:      RokuPowerService (derived from CppWindowsService)
* Copyright (c) Microsoft Corporation.
* 
* Derivative work created by Kris Farrin, 2018
* Changes noted in comments (search for RokuPowerService)
* 
* The file declares functions that install and uninstall the service.
* 
* This source is subject to the Microsoft Limited Public License v1.1.
* See http://www.microsoft.com/en-us/openness/resources/licenses.aspx#MPL.
* All other rights reserved.
* 
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, 
* EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED 
* WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
\***************************************************************************/

#pragma once


//
//   FUNCTION: InstallService
//
//   PURPOSE: Install the current application as a service to the local 
//   service control manager database.
//
//   PARAMETERS:
//   * pszServiceName - the name of the service to be installed
//   * pszDisplayName - the display name of the service
//   * dwStartType - the service start option. This parameter can be one of 
//     the following values: SERVICE_AUTO_START, SERVICE_BOOT_START, 
//     SERVICE_DEMAND_START, SERVICE_DISABLED, SERVICE_SYSTEM_START.
//   * pszDependencies - a pointer to a double null-terminated array of null-
//     separated names of services or load ordering groups that the system 
//     must start before this service.
//   * pszAccount - the name of the account under which the service runs.
//   * pszPassword - the password to the account name.
//	 * [szDescription] - Service description shown in services.msc
//                       Added and used to store DeviceIP for RokuPowerService.
//
//   NOTE: If the function fails to install the service, it prints the error 
//   in the standard output stream for users to diagnose the problem.
//
void InstallService(PWSTR pszServiceName, 
                    PWSTR pszDisplayName, 
                    DWORD dwStartType,
                    PWSTR pszDependencies, 
                    PWSTR pszAccount, 
                    PWSTR pszPassword, PWSTR szDescription = NULL, SERVICE_FAILURE_ACTIONS* servFailActions = NULL);


//
//   FUNCTION: UninstallService
//
//   PURPOSE: Stop and remove the service from the local service control 
//   manager database.
//
//   PARAMETERS: 
//   * pszServiceName - the name of the service to be removed.
//
//   NOTE: If the function fails to uninstall the service, it prints the 
//   error in the standard output stream for users to diagnose the problem.
//
void UninstallService(PWSTR pszServiceName);


//
//   FUNCTION: RestartService
//
//   PURPOSE: Stop and restart the service from the local service control 
//   manager database.
//
//   PARAMETERS: 
//   * pszServiceName - the name of the service to be restarted.
//
//   Added by Kris Farrin for RokuPowerService
//   To Do: Refactor the delete and restart functions since they are so similar
//
void RestartService(PWSTR pszServiceName);


//
//   FUNCTION: GetServiceDescription
//
//   PURPOSE: Get the service description.
//
//   PARAMETERS: 
//   * pszServiceName - the name of the service to be queried.
//
wchar_t* GetServiceDescription(PWSTR pszServiceName);
