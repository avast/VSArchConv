
#include "stdafx.h"

using namespace std;
using namespace pugi;

string strSrcPlatform("Win32"); //implicit create ARM64 from Win32 platform
string strDstPlatform("ARM64");
string strDstPlatformToolset("v141");
string strDstWindowsTargetPlatform("10.0.10586.0");
string strDstTargetMachine("MachineARM64");

const string strItemGroup("ItemGroup");
const string strProjectConfiguration("ProjectConfiguration");
const string strPlatform("Platform");
const string strPlatformToolset("PlatformToolset");
const string strWindowsTargetPlatformVersion("WindowsTargetPlatformVersion");
const string strTargetMachine("TargetMachine");
const string strTargetEnvironment("TargetEnvironment");
const string strLink("Link");
const string strBaseAddress("BaseAddress");

const string strSolutionConfigs("GlobalSection(SolutionConfigurationPlatforms)");
const string strProjectConfigs("GlobalSection(ProjectConfigurationPlatforms)");
const string strEndGlobalSection("EndGlobalSection");

const wstring strProjFileMask( L"*.vcxproj" );
const wstring strSlnFileMask( L"*.sln" );

enum PrjChanges
{
	bToolset,
	bWindowsTargetPlatform,
	bTargetMachine,
	bTargetEnvironment,
	bMaxOption
};

list<wstring> listExcludes;
list<wstring> listDirs;
list<wstring> listFiles;

template< typename Tfunc >
bool PerformForAllFilesWithMask(const wstring & strDir, const wstring & strFileMask, const Tfunc & func, bool bRecursive);

bool AddPlatformToProject(const wstring & strVcxProjFile, const vector<bool> & vecChange);
bool AddPlatformToSln(const wstring & strSlnFile);
bool RemoveBaseAddressFromProject(const wstring & strVcxProjFile, const vector<bool> & vecChange);
bool DuplicateSrcPlatformConditions(xml_node & xmlRoot, const vector<bool> & vecChange);

auto SearchForStringInList(const list<string> & listStr, const string & strFind);
auto SearchForStringInList(const list<string> & listStr, list<string>::const_iterator cstart, const string & strFind);
bool AddPlatformToSln(const wstring & strSlnFile);

int wmain()
{			
	bool bCmdLineError = false;
	bool bRecursive = true;
	
	enum Command
	{
		CommandVcxProjAddPlatform,
		CommandSlnAddPlatform,
		CommandVcxProjRemoveBaseAddress,
	} eCommand = CommandVcxProjAddPlatform;
	
	
	bool bSln = false;
		
	wstring_convert<codecvt_utf8<wchar_t>> conv;

	vector<bool> vecConfigChanges(bMaxOption, true);
	
	if ( __argc < 2 ) {
		printf("VSArchConv - converts .sln/.vcxproj to support different architecture\n" );
		printf("Copyright (C) 1988-2018 Avast Software\n");
		printf("\n");
		printf("Usage: VSArchConv.exe [/norecursive] [/sln] [/rembase] [/notoolset] [/notgtwin] [/notgtmach] [/notgtenv] [/exclude directory] [/srcplatform platform directory] [/dstplatform platform] "
			"[/dsttoolset toolset] [/dstwindows version] [/dstmachine machine]\n" );
		printf("\n");
		printf("  /norecursive   Recurse subdirectories\n");
		printf("  /sln           Convert .sln and all its projects\n");
		printf("  /rembase       Remove BaseAddress for EXEs/DLLs\n");
		printf("                 expected format: <BaseAddress>@dlls.txt,HelperDLL</BaseAddress>\n");
		printf("  /notoolset     Remove PlatformToolset\n");
		printf("  /notgtwin      Remove WindowsTargetPlatformVersion\n");
		printf("  /notgtmach     Remove TargetMachine\n");
		printf("  /notgtenv      Remove TargetEnvironment\n");
		printf("  /exclude       Exclude directory\n");
		printf("  /srcplatform   Replace default source TargetMachine (default: Win32)\n");
		printf("  /dstplatform   Replace default destination TargetMachine (default: ARM64)\n");
		printf("  /dsttoolset    Replace default destination PlatformToolset (default: v141)\n");
		printf("  /dstwindows    Replace default destination WindowsTargetPlatformVersion (default: 10.0.10586.0)\n");
		printf("  /dstmachine    Replace default destination TargetMachine (default: MachineARM64)\n");
		printf("\n");
		exit( 1 );
	}

	for (int i = 1; i < __argc; i++)
	{
		if (_wcsicmp(__wargv[i], L"/norecursive") == 0)
		{		
			bRecursive = false;
		}
		else if (_wcsicmp(__wargv[i], L"/sln") == 0)
		{
			eCommand = CommandSlnAddPlatform;
		}
		else if (_wcsicmp(__wargv[i], L"/rembase") == 0)
		{
			eCommand = CommandVcxProjRemoveBaseAddress;
		}
		else if (_wcsicmp(__wargv[i], L"/notoolset") == 0)
		{
			vecConfigChanges[bToolset] = false;
		}
		else if (_wcsicmp(__wargv[i], L"/notgtwin") == 0)
		{
			vecConfigChanges[bWindowsTargetPlatform] = false;
		}
		else if (_wcsicmp(__wargv[i], L"/notgtmach") == 0)
		{
			vecConfigChanges[bTargetMachine] = false;
		}		
		else if (_wcsicmp(__wargv[i], L"/notgtenv") == 0)
		{
			vecConfigChanges[bTargetEnvironment] = false;
		}
		else if (_wcsicmp(__wargv[i], L"/exclude") == 0)
		{
			i++;
			if (i >= __argc) { bCmdLineError = true; break; }
			listExcludes.push_back(__wargv[i]);
		}
		else if (_wcsicmp(__wargv[i], L"/srcplatform") == 0)
		{
			i++;
			if (i >= __argc) { bCmdLineError = true; break; }
			strSrcPlatform = conv.to_bytes(__wargv[i]);
		}
		else if (_wcsicmp(__wargv[i], L"/dstplatform") == 0)
		{
			i++;
			if (i >= __argc) { bCmdLineError = true; break; }
			strDstPlatform = conv.to_bytes(__wargv[i]);
		}
		else if (_wcsicmp(__wargv[i], L"/dsttoolset") == 0)
		{
			i++;
			if (i >= __argc) { bCmdLineError = true; break; }
			strDstPlatformToolset = conv.to_bytes(__wargv[i]);
		}
		else if (_wcsicmp(__wargv[i], L"/dstwindows") == 0)
		{
			i++;
			if (i >= __argc) { bCmdLineError = true; break; }
			strDstWindowsTargetPlatform = conv.to_bytes(__wargv[i]);
		}
		else if (_wcsicmp(__wargv[i], L"/dstmachine") == 0)
		{
			i++;
			if (i >= __argc) { bCmdLineError = true; break; }
			strDstTargetMachine = conv.to_bytes(__wargv[i]);
		}
		else
		{
			auto attrs = GetFileAttributes(__wargv[i]);
			if (attrs == INVALID_FILE_ATTRIBUTES)
			{
				wcout << __wargv[i] << L" is neither file nor directory" << std::endl;
				bCmdLineError = true;
				break;
			}

			if (attrs & FILE_ATTRIBUTE_DIRECTORY)
				listDirs.push_back(__wargv[i]);
			else
				listFiles.push_back(__wargv[i]);
		}
	}
	
	if (bCmdLineError)
	{
		wcout << L"Invalid command line" << std::endl;
		return 1;
	}

	if (listDirs.empty() && listFiles.empty())
	{
		wcout << L"No input files/directories specified" << std::endl;
		return 1;
	}

	bool retval = false;
	
	if (eCommand == CommandSlnAddPlatform)
	{
		for (const auto & strDir : listDirs)
			retval = PerformForAllFilesWithMask(strDir, strSlnFileMask,
				[](const wstring & strSlnFile) -> bool
				{
					return AddPlatformToSln(strSlnFile);
				},
				bRecursive
			);

		for (const auto & strProj : listFiles)
			retval = AddPlatformToSln(strProj);
	}	
	else if (eCommand == CommandVcxProjAddPlatform)
	{
		for (const auto & strDir : listDirs)
			retval = PerformForAllFilesWithMask(strDir, strProjFileMask,
				[&vecConfigChanges](const wstring & strVcxProjFile) -> bool
				{
					return AddPlatformToProject(strVcxProjFile, vecConfigChanges);
				},
				bRecursive
			);

		for (const auto & strProj : listFiles)
			retval = AddPlatformToProject(strProj, vecConfigChanges);
	}
	else if (eCommand == CommandVcxProjRemoveBaseAddress)
	{
		for (const auto & strDir : listDirs)
			retval = PerformForAllFilesWithMask(strDir, strProjFileMask,
				[&vecConfigChanges](const wstring & strVcxProjFile) -> bool
			{
				return RemoveBaseAddressFromProject(strVcxProjFile, vecConfigChanges);
			},
				bRecursive
			);

		for (const auto & strProj : listFiles)
			retval = RemoveBaseAddressFromProject(strProj, vecConfigChanges);
	}
	
	return 0;
}

template< typename Tfunc >
bool PerformForAllFilesWithMask(const wstring & strDir, const wstring & strFileMask, const Tfunc & func, bool bRecursive)
{
	bool retval = true;
	bool excluded;
	wstring strDirMask;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	WIN32_FIND_DATA wfd;

	do 
	{			
		
		excluded = false;
		//check excludes
		for (const auto & strExcl : listExcludes)
			if (_wcsicmp(strDir.c_str(), strExcl.c_str()) == 0) { excluded = true; break; } //excluded
		if (excluded) break;
		
		strDirMask = strDir + L"\\*";
			
		hFind = FindFirstFile(strDirMask.c_str(), &wfd);
		if (hFind == INVALID_HANDLE_VALUE) { retval = false; break; }

		do 
		{
			wstring strName = wfd.cFileName;
			if (strName == L"." || strName == L"..") continue;
			
			if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (bRecursive)
				{
					strDirMask = strDir + L"\\" + strName;
					retval = PerformForAllFilesWithMask(strDirMask, strFileMask, func, bRecursive);
					if (!retval) break;
				}
			}
			else if (PathMatchSpec(wfd.cFileName, strFileMask.c_str()))
			{
				strName = strDir + L"\\" + strName;

				excluded = false;
				for (const auto & strExcl : listExcludes)
					if (_wcsicmp(strName.c_str(), strExcl.c_str()) == 0) { excluded = true; break; }
				if (excluded) continue;

				(void) func(strName);
			}
		}
		while (FindNextFile(hFind, &wfd));
	}
	while (0);

	if (hFind != INVALID_HANDLE_VALUE) FindClose(hFind);

	return retval;
}

bool ReplaceElementsData(xml_node & xmlRoot, const string & strElementName, const string & strElementData, bool bRecursive)
{
	try
	{
		if (xmlRoot.type() != node_element) return true;
		
		for (xml_node xmlChild = xmlRoot.first_child(); xmlChild; xmlChild = xmlChild.next_sibling())
		{
			if (xmlChild.type() != node_element) continue;
			if (strElementName == xmlChild.name())
			{
				for (xml_node xmlChildValue = xmlChild.first_child(); xmlChildValue; xmlChildValue = xmlChildValue.next_sibling())
					if (xmlChildValue.type() == node_pcdata)
					{
						xmlChildValue.set_value(strElementData.c_str());
						break;
					}
			}
			else if (bRecursive)
			{
				if (!ReplaceElementsData(xmlChild, strElementName, strElementData, bRecursive))
					return false;
			}
		}
	}	
	catch (std::exception & e)
	{
		wcout << L"std Exception encountered: " << e.what() << std::endl;
		return false;
	}

	return true;
}

bool DuplicateSrcPlatformConditions(xml_node & xmlRoot, const vector<bool> & vecChange)
{
	try
	{	
		if (xmlRoot.type() != node_element) return true;
	
		for (xml_node rootChild = xmlRoot.first_child(); rootChild; rootChild = rootChild.next_sibling())
		{
			if (rootChild.type() != node_element) continue;
															
			xml_attribute xmlAttribute = rootChild.attribute("Condition");
			if (!xmlAttribute)
			{
				if (!DuplicateSrcPlatformConditions(rootChild, vecChange)) return false; //go deeper				
				continue;
			}
			string strAttribute = xmlAttribute.value();

			if (strAttribute.find("$(Platform)") == strAttribute.npos) continue;
			auto posSrcPlatform = strAttribute.find(strSrcPlatform);
			if (posSrcPlatform == strAttribute.npos) continue;

			strAttribute.replace(posSrcPlatform, strSrcPlatform.length(), strDstPlatform);

			xml_node xmlElementCopy = xmlRoot.insert_copy_after(rootChild, rootChild);

			xmlAttribute = xmlElementCopy.attribute("Condition");
			if (!xmlAttribute)
				throw "Condition attribute missing in copy";

			xmlAttribute.set_value(strAttribute.c_str());

			if (vecChange[bTargetMachine])
				if (!ReplaceElementsData(xmlElementCopy, strTargetMachine, strDstTargetMachine, true))
					throw "Unable to replace elements target machine";

			if (vecChange[bTargetEnvironment])
				if (!ReplaceElementsData(xmlElementCopy, strTargetEnvironment, strDstPlatform, true))
					throw "Unable to replace elements target environment";
			
			if (vecChange[bToolset] || vecChange[bWindowsTargetPlatform])
			{
				xml_node xmlPlatformToolset = xmlElementCopy.find_child(
					[](const xml_node & node)
					{
						return strPlatformToolset == node.name();
					}
				);

				if (xmlPlatformToolset)
				{
					if (vecChange[bToolset])
					{
						for (xml_node xmlPlatformToolsetValue = xmlPlatformToolset.first_child(); xmlPlatformToolsetValue; xmlPlatformToolsetValue = xmlPlatformToolsetValue.next_sibling())
							if (xmlPlatformToolsetValue.type() == node_pcdata)
							{
								xmlPlatformToolsetValue.set_value(strDstPlatformToolset.c_str());
								break;
							}
					}
								
					if (vecChange[bWindowsTargetPlatform])
					{
						xml_node xmlWindowsTargetPlatform = xmlElementCopy.find_child(
							[](const xml_node & node)
							{
								return strWindowsTargetPlatformVersion == node.name();
							}
						);

						if (xmlWindowsTargetPlatform)
						{
							for (xml_node xmlWindowsTargetPlatformValue = xmlWindowsTargetPlatform.first_child(); xmlWindowsTargetPlatformValue; xmlWindowsTargetPlatformValue = xmlWindowsTargetPlatformValue.next_sibling())
								if (xmlWindowsTargetPlatformValue.type() == node_pcdata)
								{
									xmlWindowsTargetPlatformValue.set_value(strDstWindowsTargetPlatform.c_str());
									break;
								}
						}
						else
						{
							xmlWindowsTargetPlatform = xmlElementCopy.append_child(strWindowsTargetPlatformVersion.c_str());
							xml_node xmlWindowsTargetPlatformValue = xmlWindowsTargetPlatform.append_child(node_pcdata);
							xmlWindowsTargetPlatformValue.set_value(strDstWindowsTargetPlatform.c_str());
						}
					}
				}
			
			}

			
		}
	}
	catch (wchar_t const * szErr)
	{
		wcout << L"Exception encountered: " << szErr << std::endl;
		return false;
	}
	catch (std::exception & e)
	{
		wcout << L"std Exception encountered: " << e.what() << std::endl;
		return false;
	}

	return true;
}


bool AddPlatformToProject(const wstring & strVcxProjFile, const vector<bool> & vecChange)
{	
	xml_document xmlProj;
	xml_node xmlRoot;	
	list<string> vecSrcConfigs;
	list<string> vecDstConfigs;		
	bool bSrcPlatformFound = false;
	bool bDstPlatformFound = false;
	
	try
	{
		if (strVcxProjFile.empty()) throw L"Project file not specified";

		xml_parse_result xmlParse = xmlProj.load_file(strVcxProjFile.c_str());
		if (xmlParse.status != status_ok) throw L"Unable to load project file";

		xmlRoot = xmlProj.first_child();
		if (!xmlRoot) throw L"Project file has no root element";


		xml_node xmlProjectConfigurations = xmlRoot.find_child_by_attribute("Label", "ProjectConfigurations");
		if (!xmlProjectConfigurations) throw L"Unable to find ProjectConfigurations ItemGroup";
		if (strItemGroup != xmlProjectConfigurations.name()) throw L"Wrong element name specified for ProjectConfigurations";
			
		for (xml_node pcChild = xmlProjectConfigurations.first_child(); pcChild; pcChild = pcChild.next_sibling())
		{									
			if (pcChild.type() != node_element) continue;
			if (strProjectConfiguration != pcChild.name()) continue;

			xml_attribute xmlAttribute = pcChild.attribute("Include");
			if (!xmlAttribute) continue;

			string strInclude = xmlAttribute.value();

			for (xml_node pcChildIn = pcChild.first_child(); pcChildIn; pcChildIn = pcChildIn.next_sibling())
			{
				if (pcChildIn.type() != node_element) continue;
				if (strPlatform != pcChildIn.name()) continue;
												
				xml_node xmlPlatform;
				for (xmlPlatform = pcChildIn.first_child(); xmlPlatform; xmlPlatform = xmlPlatform.next_sibling())
					if (xmlPlatform.type() == node_pcdata) break;

				if (!xmlPlatform) continue;
				
				if (strSrcPlatform == xmlPlatform.value())
				{
					bSrcPlatformFound = true;
					if (strInclude.find("|" + strSrcPlatform) == strInclude.npos) throw L"Unable to find source platform in project configuration include";
					vecSrcConfigs.push_back(strInclude);
				}
				else if (strDstPlatform == xmlPlatform.value())
					bDstPlatformFound = true;
			}
		}
		
		if (!bSrcPlatformFound)
		{
			wcout << L"File: " << strVcxProjFile << L" doesn't contain source destination platform configuration" << std::endl;
			return false;
		}
		
		
		if (bDstPlatformFound) 
		{
			wcout << L"File: " << strVcxProjFile << L" already contains destination platform configuration" << std::endl;
			return true;
		}
		
		//create new project configuration
		for (const string & strSrcConfig : vecSrcConfigs)
		{
			string strDstConfig = strSrcConfig;
			auto pos = strDstConfig.find_last_of("|");
			if (pos == strDstConfig.npos)
				throw L"Unable to find | character in project configuration include";

			strDstConfig.replace(pos + 1, strSrcPlatform.length(), strDstPlatform);
			vecDstConfigs.push_back(strDstConfig);

			xml_node xmlProjectConfig = xmlProjectConfigurations.find_child_by_attribute("Include", strSrcConfig.c_str());
			if (!xmlProjectConfig)
				throw L"Unable to find corresponding Include atribute";

			xml_node xmlProjectConfigCopy = xmlProjectConfigurations.insert_copy_after(xmlProjectConfig, xmlProjectConfig);
			xml_attribute xmlAttribute = xmlProjectConfigCopy.attribute("Include");
			if (!xmlAttribute )
				throw L"Unable to find Include attribute in node copy";
			xmlAttribute.set_value(strDstConfig.c_str());

			xml_node xmlPlatform = xmlProjectConfigCopy.find_child(
				[](const xml_node & node)
				{					
					return strPlatform == node.name();						
				}
			);

			if (!xmlPlatform)
				throw L"Unable to find Platform element in node copy";
						
			for (xml_node xmlPlatformValue = xmlPlatform.first_child(); xmlPlatformValue; xmlPlatformValue = xmlPlatformValue.next_sibling())
				if (xmlPlatformValue.type() == node_pcdata)
				{				
					xmlPlatformValue.set_value(strDstPlatform.c_str());
					break;
				}
		}

		//duplicate all src platform conditions to be dst as well
		if (!DuplicateSrcPlatformConditions(xmlRoot, vecChange)) return false;
		
		unsigned flags = format_indent;
		if (xmlParse.encoding == encoding_utf8)
			flags |= format_write_bom;
		
		if (!xmlProj.save_file(strVcxProjFile.c_str(), PUGIXML_TEXT("  "), format_indent | format_write_bom, xmlParse.encoding))
			throw L"Unable to save project file";
	}
	catch ( wchar_t const * szErr )
	{
		wcout << L"Exception encountered: " << szErr << L" File: " << strVcxProjFile << std::endl;
		return false;
	}
	catch (std::exception & e)
	{
		wcout << L"std Exception encountered: " << e.what() << L" File: " << strVcxProjFile << std::endl;
		return false;
	}

	return true;
}

auto SearchForStringInList(const list<string> & listStr, const string & strFind)
{		
	auto citer = listStr.cbegin();
	
	for (; citer != listStr.cend(); citer++)	
		if (citer->find(strFind) != citer->npos) break;

	return citer;
}

auto SearchForStringInList(const list<string> & listStr, list<string>::const_iterator cstart, const string & strFind)
{
	auto citer = cstart;	
	for (; citer != listStr.cend(); citer++)
		if (citer->find(strFind) != citer->npos) break;			
	return citer;
}

void AddPlatformToRange(list<string> & listStr, list<string>::const_iterator cstart, list<string>::const_iterator cend)
{
	for (auto citer = cstart; citer != cend;)
	{
		if (citer->find("|" + strSrcPlatform) != citer->npos)
		{

			string strNewLine = *citer;
			citer++;

			string::size_type pos = 0;
			while ((pos = strNewLine.find(strSrcPlatform, pos)) != strNewLine.npos)
			{
				strNewLine.replace(pos, strSrcPlatform.length(), strDstPlatform);
				pos += strDstPlatform.length();
			}

			citer = listStr.insert(citer, std::move(strNewLine));
		}
		else
			citer++;
	}
}


bool AddPlatformToSln(const wstring & strSlnFile)
{
	list<string> strSlnContent;
	bool bSrcPlatformFound = false;
	bool bDstPlatformFound = false;
	
	try
	{
		{
			//load sln file into list of strings
			string strLine; 
			ifstream ifsSlnFile(strSlnFile, ios::binary);
			if (!ifsSlnFile.is_open())
				throw L"Unable to load solution file";
			
			while (getline(ifsSlnFile, strLine, '\n'))
				strSlnContent.push_back(strLine);
		}

		auto citerConfigStart = SearchForStringInList(strSlnContent, strSolutionConfigs);
		if (citerConfigStart == strSlnContent.cend())
			throw L"Unable to find solution configurations section";
		
		citerConfigStart++;
		
		auto citerConfigEnd = SearchForStringInList(strSlnContent, citerConfigStart, strEndGlobalSection);
		if (citerConfigEnd == strSlnContent.cend())
			throw L"Unable to find solution configurations section terminator";

		for (auto citer = citerConfigStart; citer != citerConfigEnd; citer++)
		{
			if (citer->find("|" + strSrcPlatform) != citer->npos)
				bSrcPlatformFound = true;

			if (citer->find("|" + strDstPlatform) != citer->npos)
				bDstPlatformFound = true;
		}

		if (!bSrcPlatformFound)
		{
			wcout << L"File: " << strSlnFile << L" doesn't contain source destination platform configuration" << std::endl;
			return false;
		}

		if (bDstPlatformFound)
		{
			wcout << L"File: " << strSlnFile << L" already contains destination platform configuration" << std::endl;
			return true;
		}

		AddPlatformToRange(strSlnContent, citerConfigStart, citerConfigEnd);


		citerConfigStart = SearchForStringInList(strSlnContent, citerConfigEnd, strProjectConfigs);
		if (citerConfigStart == strSlnContent.cend())
			throw L"Unable to find projects configurations section";

		citerConfigStart++;

		citerConfigEnd = SearchForStringInList(strSlnContent, citerConfigStart, strEndGlobalSection);
		if (citerConfigEnd == strSlnContent.cend())
			throw L"Unable to find projects configurations section terminator";

		AddPlatformToRange(strSlnContent, citerConfigStart, citerConfigEnd);

		{
			//write sln file back
			ofstream ofsSlnFile(strSlnFile, ios::binary);
			if (!ofsSlnFile.is_open())
				throw L"Unable to save solution file";

			for (const auto & strLine : strSlnContent)
				ofsSlnFile << strLine << '\n';			
		}

	}	
	catch (wchar_t const * szErr)
	{
		wcout << L"Exception encountered: " << szErr << L" File: " << strSlnFile << std::endl;
		return false;
	}
	catch (std::exception & e)
	{
		wcout << L"STD Exception encountered: " << e.what() << L" File: " << strSlnFile << std::endl;
		return false;
	}

	return true;
}

bool RemoveBaseAddressFromElement(xml_node & xmlRoot, const vector<bool> & vecChange, bool & bModified)
{
	try
	{
		if (xmlRoot.type() != node_element) return true;

		if (xmlRoot.name() == strLink)
		{
			xml_node xmlBaseAddress = xmlRoot.find_child(
				[](const xml_node & node)
				{
					return strBaseAddress == node.name();
				}
			);

			if (xmlBaseAddress)
			{
				string strValue = xmlBaseAddress.child_value();
				if (strValue.find("dlls.txt") != strValue.npos)
				{
					xmlRoot.remove_child(xmlBaseAddress);
					bModified = true;
				}
			}
		}
		else
		{
			for (xml_node rootChild = xmlRoot.first_child(); rootChild; rootChild = rootChild.next_sibling())
			{
				if (rootChild.type() != node_element) continue;
				if (!RemoveBaseAddressFromElement(rootChild, vecChange, bModified)) return false;
			}
		}
	}
	catch (wchar_t const * szErr)
	{
		wcout << L"Exception encountered: " << szErr << std::endl;
		return false;
	}
	catch (std::exception & e)
	{
		wcout << L"std Exception encountered: " << e.what() << std::endl;
		return false;
	}

	return true;
}

bool RemoveBaseAddressFromProject(const wstring & strVcxProjFile, const vector<bool> & vecChange)
{
	xml_document xmlProj;
	xml_node xmlRoot;
	bool bModified = false;
	
	try
	{
		if (strVcxProjFile.empty()) throw L"Project file not specified";

		xml_parse_result xmlParse = xmlProj.load_file(strVcxProjFile.c_str());
		if (xmlParse.status != status_ok) throw L"Unable to load project file";

		xmlRoot = xmlProj.first_child();
		if (!xmlRoot) throw L"Project file has no root element";

		if (!RemoveBaseAddressFromElement(xmlRoot, vecChange, bModified)) return false;

		unsigned flags = format_indent;
		if (xmlParse.encoding == encoding_utf8)
			flags |= format_write_bom;

		if (bModified)
			if (!xmlProj.save_file(strVcxProjFile.c_str(), PUGIXML_TEXT("  "), format_indent | format_write_bom, xmlParse.encoding))
				throw L"Unable to save project file";
	}
	catch (wchar_t const * szErr)
	{
		wcout << L"Exception encountered: " << szErr << L" File: " << strVcxProjFile << std::endl;
		return false;
	}
	catch (std::exception & e)
	{
		wcout << L"std Exception encountered: " << e.what() << L" File: " << strVcxProjFile << std::endl;
		return false;
	}

	return true;
}