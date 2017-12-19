
/* ===================================================================== */
/* Imported Headers														 */
/* ===================================================================== */

#include "pin.H"

#include <iostream>
#include <fstream>
#include <string>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <list>


/* ===================================================================== */
/* Global Variables 												     */
/* ===================================================================== */

std::ofstream TraceFile;
std::list<const char*> lst;

/* ===================================================================== */
/* Methods to enumerate exported functions from images (Module) 		 */
/* ===================================================================== */

void EnumExportedFunctions(const char *, void(*callback)(char*));

int Rva2Offset(unsigned int);

typedef struct {
	unsigned char Name[8];
	unsigned int VirtualSize;
	unsigned int VirtualAddress;
	unsigned int SizeOfRawData;
	unsigned int PointerToRawData;
	unsigned int PointerToRelocations;
	unsigned int PointerToLineNumbers;
	unsigned short NumberOfRelocations;
	unsigned short NumberOfLineNumbers;
	unsigned int Characteristics;
} sectionHeader;

sectionHeader *sections;
unsigned int NumberOfSections = 0;

int Rva2Offset(unsigned int rva) {
	int i = 0;

	for (i = 0; i < NumberOfSections; i++) {
		unsigned int x = sections[i].VirtualAddress + sections[i].SizeOfRawData;

		if (x >= rva) {
			return sections[i].PointerToRawData + (rva + sections[i].SizeOfRawData) - x;
		}
	}

	return -1;
}

void EnumExportedFunctions(const char *szFilename, void(*callback)(char*)) {
	FILE *hFile = fopen(szFilename, "rb");

	if (hFile != NULL) {
		if (fgetc(hFile) == 'M' && fgetc(hFile) == 'Z') {
			unsigned int e_lfanew = 0;
			unsigned int NumberOfRvaAndSizes = 0;
			unsigned int ExportVirtualAddress = 0;
			unsigned int ExportSize = 0;
			int i = 0;

			fseek(hFile, 0x3C, SEEK_SET);
			fread(&e_lfanew, 4, 1, hFile);
			fseek(hFile, e_lfanew + 6, SEEK_SET);
			fread(&NumberOfSections, 2, 1, hFile);
			fseek(hFile, 108, SEEK_CUR);
			fread(&NumberOfRvaAndSizes, 4, 1, hFile);

			if (NumberOfRvaAndSizes == 16) {
				fread(&ExportVirtualAddress, 4, 1, hFile);
				fread(&ExportSize, 4, 1, hFile);

				if (ExportVirtualAddress > 0 && ExportSize > 0) {
					fseek(hFile, 120, SEEK_CUR);

					if (NumberOfSections > 0) {
						sections = (sectionHeader *)malloc(NumberOfSections * sizeof(sectionHeader));

						for (i = 0; i < NumberOfSections; i++) {
							fread(sections[i].Name, 8, 1, hFile);
							fread(&sections[i].VirtualSize, 4, 1, hFile);
							fread(&sections[i].VirtualAddress, 4, 1, hFile);
							fread(&sections[i].SizeOfRawData, 4, 1, hFile);
							fread(&sections[i].PointerToRawData, 4, 1, hFile);
							fread(&sections[i].PointerToRelocations, 4, 1, hFile);
							fread(&sections[i].PointerToLineNumbers, 4, 1, hFile);
							fread(&sections[i].NumberOfRelocations, 2, 1, hFile);
							fread(&sections[i].NumberOfLineNumbers, 2, 1, hFile);
							fread(&sections[i].Characteristics, 4, 1, hFile);
						}

						unsigned int NumberOfNames = 0;
						unsigned int AddressOfNames = 0;

						int offset = Rva2Offset(ExportVirtualAddress);
						fseek(hFile, offset + 24, SEEK_SET);
						fread(&NumberOfNames, 4, 1, hFile);

						fseek(hFile, 4, SEEK_CUR);
						fread(&AddressOfNames, 4, 1, hFile);

						unsigned int namesOffset = Rva2Offset(AddressOfNames), pos = 0;
						fseek(hFile, namesOffset, SEEK_SET);

						for (i = 0; i < NumberOfNames; i++) {
							unsigned int y = 0;
							fread(&y, 4, 1, hFile);
							pos = ftell(hFile);
							fseek(hFile, Rva2Offset(y), SEEK_SET);

							char c = fgetc(hFile);
							int szNameLen = 0;

							while (c != '\0') {
								c = fgetc(hFile);
								szNameLen++;
							}

							fseek(hFile, (-szNameLen) - 1, SEEK_CUR);
							char* szName = (char*)calloc(szNameLen + 1, 1);
							fread(szName, szNameLen, 1, hFile);

							callback(szName);

							fseek(hFile, pos, SEEK_SET);
						}
					}
				}
			}
		}

		fclose(hFile);
	}
}


/* ===================================================================== */
/* It'll add exported functions into the lst							 */
/* ===================================================================== */


void mycallback(char* szName) {
	lst.push_back(szName);
}

/* ===================================================================== */
/* Commandline Switches													 */
/* ===================================================================== */

// If you don't specify the the -o in command line, then the default output will be saved in pinitor.txt
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "pinitor.out", "specify trace file name");

/* ===================================================================== */


/* ===================================================================== */
/* Showing function names and their aruments                             */
/* ===================================================================== */
 
// As I mentioned in the my blog at http://rayanfam.com/topics/pinitor/  I have know idea about 
// how to get the exact number of Arguments which is passed to the specific function except using
// a data file which previously stored every functions entrance argument, so if you know any other
// way then please tell me about it in my blog.

VOID AddInvokeFunctionToFile(CHAR * name, ADDRINT Arg1,ADDRINT Arg2 , ADDRINT Arg3, ADDRINT Arg4, ADDRINT Arg5, ADDRINT Arg6, ADDRINT Arg7, ADDRINT Arg8, ADDRINT Arg9, ADDRINT Arg10, ADDRINT Arg11, ADDRINT Arg12)
{
    TraceFile << name << "(" << Arg1 << ","<< Arg2 << "," << Arg3  << "," <<Arg4 << "," << Arg5 << "," << Arg6 << "," << Arg7 << "," << Arg8 << "," << Arg9 << "," << Arg10 << "," <<Arg11 << "," <<Arg12<< ")" << endl;
}

VOID AddReturnResultsToFile(ADDRINT ret)
{
    TraceFile << "  returns " << ret << endl;
}


/* ===================================================================== */
/* Instrumentation routines                                              */
/* ===================================================================== */
   
VOID Image(IMG img, VOID *v)
{
	// Showing which modules have been loaded
	std::cout << endl << "====================================================================="<<endl;
	std::cout << "Module loaded : " << IMG_Name(img);
	std::cout << endl << "====================================================================="<<endl;	
	
	// Call EnumExportedFunctions for every new loaded modules to enumerate every exported functions and create call to them
	EnumExportedFunctions(IMG_Name(img).c_str(), mycallback);

	// Walk through all the enumerated functions
	
	for (auto ci = lst.begin(); ci != lst.end(); ++ci)
	{		
	// Get a handle from the function by its name
    RTN hFunc = RTN_FindByName(img, *ci); 
	
	// Check if its valid
		if (RTN_Valid(hFunc))
		{
			// Open the handle
			RTN_Open(hFunc);

			// Insert a call before the function to be notified when a specific function called.
			RTN_InsertCall(hFunc, IPOINT_BEFORE, (AFUNPTR)AddInvokeFunctionToFile,
                        IARG_ADDRINT, *ci,
					    IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
					    IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
					    IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
					    IARG_END);
					   
			
			// Insert call after return		   
			RTN_InsertCall(hFunc, IPOINT_AFTER, (AFUNPTR)AddReturnResultsToFile,
                       IARG_FUNCRET_EXITPOINT_VALUE, IARG_END);
			
			// Close the handle
			RTN_Close(hFunc);
		}
	}
	
}

/* ===================================================================== */
/* File Operation				                                         */
/* ===================================================================== */

VOID Fini(INT32 code, VOID *v)
{
	//Close the file
    TraceFile.close();
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */
   
INT32 Usage()
{

	// Print the banner
	cerr << endl << "______ _       _ _             ";
	cerr << endl << "| ___ (_)     (_) |            ";
	cerr << endl << "| |_/ /_ _ __  _| |_ ___  _ __ ";
	cerr << endl << "|  __/| | '_ \\| | __/ _ \\| '__|";
	cerr << endl << "| |   | | | | | | || (_) | |   ";
	cerr << endl << "\\_|   |_|_| |_|_|\\__\\___/|_|   ";
                               
	cerr << endl<< endl << "Binvoke Pinitor [Version : 1.0] - An API Monitor Based on Pin" << endl <<endl;
	cerr << "Source code is Available at : https://modules.binvoke.com/pinitor" << endl;
    cerr << "This tool produces a trace of every calls to every exported Windows API" << endl;
	cerr << "It automatically insert call to every functions in a dll even if they're not native API" << endl << endl;
	cerr << "	Usage : Pin.exe -t Pinitor.dll -o output.txt -- Application.exe <Args to Application>" << endl << endl;
	cerr << "--------------------------------------------------------------------------------------------" << endl;
    cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
	Usage();
    // Initialize pin & symbol manager
    PIN_InitSymbols();
	
    if(PIN_Init(argc,argv))
    {
		//Prints the usage 
        return Usage();
    }
    
    // Write to a file since cout and cerr maybe closed by the application
    TraceFile.open(KnobOutputFile.Value().c_str());
    TraceFile << hex;
    TraceFile.setf(ios::showbase);
    
    // Register Image to be called to instrument functions.
    IMG_AddInstrumentFunction(Image, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
}

/* ===================================================================== */
/* eof 																     */
/* ===================================================================== */
