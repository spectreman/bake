#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <map>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>

typedef std::string            String;
typedef std::set<String>       SetS;
typedef std::set<int>          SetI;
typedef std::vector<String>    VecS;
typedef std::vector<VecS>      VecVecS;
typedef std::map<int, String>  MapIS;
typedef std::map<String, VecS> MapSV;

// Globals
VecS args;
VecVecS lines;
MapSV variables;
VecS inclDirs;
String RecipeName;
String Prefix;

/////////////
// Helpers //
/////////////

// Max
int Max(int a, int b);

// Join Paths
String Join(const String& a, const String& b);

// Join Paths
String Join(const String& a, const String& b, const String& c);

// Get Directory
String GetDir(const String& path);

// Concatenate VecS
String Concat(const VecS& vecS);

// Concatenate SetS
String Concat(const SetS& setS);

// Split
VecS Split(const String& line);

// Display SetS
String ToStr(const SetS& setS);

// Ends-With
bool EndsWith(const String& str, const String& ending);

// File-Exists
bool FileExists(const String& path);

// File Modification Date
int GetFileModTm(const String& filename);

// File Modification Date
int GetFileModTm(const SetS& filenames);

// Is Option On
bool IsOn(const String& key);

// Has Option
bool HasOpt(const String& key);

// Get Option
String GetOpt(const String& key);

// Get Value from Recipe
String GetVal(const String& key);

// Get Values from Recipe
VecS GetVals(const String& key);

// Get Multiple Values from Recipe
VecVecS GetValsM(const String& key);

// Get Set of Direct Includes
SetS GetIncls(const String& file);

// Get Set of Direct and Implied Includes
SetS GetAllIncls(const String& file);

// Make-Directory
void MkDir(const String& dir);

// Chop-Ending
String ChopEnd(const String& str, int end);

// Spawn Set of Commands
void Spawn(const VecS& cmds, int nSpawn);

// List Files in a Directory
VecS ListFiles(const String& dir);

// System Call
int System(const String& cmd);

// Get All Libraries
SetS GetLibFiles();

// Colors
String FgOn(int color);
String FgOff();
String FgRed();
String FgBlu();
String FgGrn();
String FgYlw();
String FgSky();
String FgOrg();

// Dashes
String Dashes(int n);

// Star
String Star();

// Display
void Display(const String& label, const String& target, const String& detail);

// First
struct First { bool done; First() : done(false) {} operator bool() { if (done) return false; return (done = true); } };


//////////
// Main //
//////////

int main(int argc, char* argv[])
{
    // Get Args
    for (int a = 1; a < argc; a++)
    {
        args.push_back(argv[a]);
    }

    // Help
    if (IsOn("h"))
    {
        std::cerr << std::endl;
        std::cerr << "Usage: bake [clean]" << std::endl;
        std::cerr << "------------" << std::endl;
        std::cerr << "-h help"      << std::endl;
        std::cerr << "-r=Recipe.cfg (Default is Recipe.cfg)" << std::endl;
        std::cerr << "-j=SpawnSize  (Default is 1)" << std::endl;
        std::cerr << std::endl;
        exit(0);
    }

    // Default Recipe
    String pRecipe = "Recipe.cfg";

    // Override Recipe
    if (HasOpt("r"))
    {
        pRecipe = GetOpt("r");
    }

    // Spawn Size
    int pSpawn = 1;
    if (HasOpt("j"))
    {
        pSpawn = Max(atoi(GetOpt("j").c_str()), 1);
    }

    ////////////
    // Recipe //
    ////////////

    // Open Recipe
    std::ifstream file(pRecipe.c_str());
    if (!file)
    {
        std::cerr << "Can't open recipe: " << pRecipe << std::endl;
        exit(1);
    }

    // Store Lines
    String line;
    while (getline(file, line))
    {
        // Skip Blanks and Comments
        if (line.empty() || line[0] == '#')
        {
            continue;
        }

        // Tokens
        VecS tokens = Split(line);

        if (!tokens.empty())
        {
            // Defines
            if (tokens[0] == "Define")
            {
                // Sufficient Tokens
                if (tokens.size() >= 2)
                {
                    // Variable Name
                    String name = tokens[1];

                    // Variable Value
                    VecS value;
                    for (int t = 2; t < tokens.size(); t++)
                    {
                        // Store Variable
                        variables[name].push_back(tokens[t]);
                    }
                }
            }
            // Normal Line
            else
            {
                lines.push_back(tokens);
            }
        }
    }

    // Substitute Variables
    VecVecS replaced;

    // For Each Line
    for (int i = 0; i < lines.size(); i++)
    {
        // Replaced Tokens
        VecS repTokens;

        VecS tokens = lines[i];
        for (int t = 0; t < tokens.size(); t++)
        {
            // Check For Variable
            String token = tokens[t];
            if (!token.empty() && token[0] == '$')
            {
                // Variable Name
                String name = token.substr(1, token.size() - 1);

                // Replace Token With Value
                MapSV::iterator find = variables.find(name);
                if (find != variables.end())
                {
					for (VecS::const_iterator v = find->second.begin(); v != find->second.end(); ++v)
					{
						repTokens.push_back(*v);
					}
                }
            }
            else
            {
                // Store Token
                repTokens.push_back(token);
            }
        }

        // Add Line
        replaced.push_back(repTokens);
    }

    // Replace
    lines = replaced;

    // Clean - Special Processing
    if (args.size() >= 1 && args[0] == "clean")
    {
        // Remove Directories
        String dirsForRemoval;
        dirsForRemoval += " " + GetVal("ObjectBinDir");

        VecVecS appDescs = GetValsM("AppDir");

        // For Each Application Description
        for (VecVecS::iterator a = appDescs.begin(); a != appDescs.end(); ++a)
        {
            // Application Description
            VecS appDesc = *a;

            // Validate Description
            if (appDesc.size() != 3 || appDesc[1] != "=>")
            {
                std::cerr << "Bad format in Recipe for AppDir, must be of the form 'appDir => binDir' not " << Concat(appDesc) << std::endl;
                exit(1);
            }

            // Application Bin Directories
            String pAppBinDir = appDesc[2];
            dirsForRemoval += " " + pAppBinDir;
        }


        // Unit-Test Descriptions
        VecVecS unitDescs = GetValsM("UnitTestDir");

        // For Each Unit-Test Description
        for (VecVecS::iterator u = unitDescs.begin(); u != unitDescs.end(); ++u)
        {
            // Unit-Test Description
            VecS unitDesc = *u;

            // Validate Description
            if (unitDesc.size() != 3 || unitDesc[1] != "=>")
            {
                std::cerr << "Bad format in Recipe for UnitTestDir, must be of the form 'unitDir => binDir' not " << Concat(unitDesc) << std::endl;
                exit(1);
            }

            // Unit-Test Directories
            String pUnitBinDir = unitDesc[2];
            dirsForRemoval += " " + pUnitBinDir;
        }

        String rmDirCmd = "rm -rf " + dirsForRemoval;
        std::cout << Star() << "Executing: " << rmDirCmd << std::endl;
        system(rmDirCmd.c_str());

        String rmScriptCmd = "rm -f " + GetVal("UnitTestScript");
        std::cout << Star() << "Executing: " << rmScriptCmd << std::endl;
        system(rmScriptCmd.c_str());

        String rmLibArcCmd = "rm -f " + GetVal("ObjectLibArc");
        std::cout << Star() << "Executing: " << rmLibArcCmd << std::endl;
        system(rmLibArcCmd.c_str());

        // Finished
        exit(0);
    }

    /////////////////
    // Recipe Name //
    /////////////////

    Prefix = FgSky() + "* Bake: " + FgOff() + FgOrg() + GetVal("Name") + FgOff() + " ";
    std::cout << Prefix << std::endl;

    //////////////
    // Includes //
    //////////////

    // Include Directories
    inclDirs = GetVals("IncludeDirs");

    // Include Flags
    String includeFlags;
    for (VecS::iterator i = inclDirs.begin(); i != inclDirs.end(); ++i)
    {
        includeFlags += " -I" + *i;
    }

    // Library Directories
    VecS libDirs = GetVals("LibraryDirs");

    // Libraries
    VecS libraries = GetVals("Libraries");

    // Library Filenames (For Dependencies)
    SetS libFileNames = GetLibFiles();

    // Library Flags
    String libraryFlags;

    // Add Library Paths
    for (VecS::iterator l = libDirs.begin(); l != libDirs.end(); ++l)
    {
        libraryFlags += " -L" + *l;
    }

    // Add Libraries
    for (VecS::iterator l = libraries.begin(); l != libraries.end(); ++l)
    {
        libraryFlags += " -l" + *l;
    }

    // Compiler
    String pCompiler = GetVal("Compiler");
    String pCompPreFlags  = Concat(GetVals("CompPreFlags"));
    String pCompPostFlags = Concat(GetVals("CompPostFlags"));


    ///////////////////
    // Build Objects //
    ///////////////////

    String pObjSrcDir = GetVal("ObjectSrcDir");
    String pObjBinDir = GetVal("ObjectBinDir");
    String pObjLibArc = GetVal("ObjectLibArc");
    MkDir(pObjBinDir);

    // Build Objects
    {
        First display;

        // Objects
        SetS objects;

        // Build Commands
        VecS cmds;

        // Object Source Files
        VecS objSrcFiles = ListFiles(pObjSrcDir);

        // For Each Object Source File
        for (VecS::iterator o = objSrcFiles.begin(); o != objSrcFiles.end(); ++o)
        {
            // Object Source File
            const String& objSrcName = *o;

            // Confirm ".cpp"
            if (EndsWith(objSrcName, ".cpp"))
            {
                String objSrcFile = Join(pObjSrcDir, objSrcName);

                // Source Exists
                if (FileExists(objSrcFile))
                {
                    // Object File
                    String objBinFile = Join(pObjBinDir, ChopEnd(objSrcName, 4) + ".o");

                    // Add To Objects
                    objects.insert(objBinFile);

                    // Need-To-Build
                    bool needToBuild = false;

                    // Object Doesn't Exist
                    if (!FileExists(objBinFile))
                    {
                        needToBuild = true;
                    }
                    // Object Exists
                    else
                    {
                        // Object Modification Time
                        int objModTime = GetFileModTm(objBinFile);

                        // Source File Modified
                        if (GetFileModTm(objSrcFile) > objModTime)
                        {
                            needToBuild = true;
                        }
                        // Check Includes
                        else
                        {
                            // Includes
                            SetS includes = GetAllIncls(objSrcFile);

                            if (GetFileModTm(includes) > objModTime)
                            {
                                needToBuild = true;
                            }
                        }
                    }

                    // Need To Build
                    if (needToBuild)
                    {
                        String cmd = pCompiler;
                        cmd += " "    + pCompPreFlags;
                        cmd += " -c " + objSrcFile;
                        cmd += " -o " + objBinFile;
                        cmd += " "    + includeFlags;
                        cmd += " "    + pCompPostFlags;

                        cmds.push_back(cmd);

                        // Display
                        if (display) Display("Building", "Objects", pObjSrcDir);
                    }
                }
			}
		}

		// Spawn Object Builds
		Spawn(cmds, pSpawn);

		// Build Object Archive (Static Library)
		if (!FileExists(pObjLibArc) || GetFileModTm(objects) > GetFileModTm(pObjLibArc))
		{
			// Display
			if (display) Display("Building", "Objects", pObjSrcDir);
			
			// Create Directory
			String objLibDir = GetDir(pObjLibArc);
			MkDir(objLibDir);

			// Archive Command
			String objLibCmd = "ar rcs " + pObjLibArc + " " + Concat(objects);
			std::cout << Prefix << FgGrn() << "Executing: " << FgOff() << objLibCmd << std::endl;
		
			// System Call
			int rc = system(objLibCmd.c_str());
			if (rc != 0)
			{
				std::cerr << "Failed to build object archive: " << pObjLibArc << std::endl;
				{
					std::cerr << "Failed to build object archive: " << pObjLibArc << std::endl;
					exit(1);
				}
			}
		}
	}

	////////////////
	// Build Apps //
	////////////////

	{
		// Build Commands
		VecS cmds;

		// Application Descriptions
		VecVecS appDescs = GetValsM("AppDir");

		// For Each Application Description
		for (VecVecS::iterator a = appDescs.begin(); a != appDescs.end(); ++a)
		{
			First display;

			// Application Description
			VecS appDesc = *a;

			// Validate Description
			if (appDesc.size() != 3 || appDesc[1] != "=>")
			{
				std::cerr << "Bad format in Recipe for AppDir, must be of the form 'appDir => binDir' not " << Concat(appDesc) << std::endl;
				exit(1);
			}

			// Application Directories
			String pAppSrcDir = appDesc[0];
			String pAppBinDir = appDesc[2];
			MkDir(pAppBinDir);

			// Application Source Files
			VecS appSrcFiles = ListFiles(pAppSrcDir);

			// For Each Application Source File
			for (VecS::iterator a = appSrcFiles.begin(); a != appSrcFiles.end(); ++a)
			{
				// Only C++ Files
				if (!EndsWith(*a, ".cpp"))
				{
					continue;
				}

				// Application Source File
				String appSrcFile = Join(pAppSrcDir, *a);

				// Application Binary File
				String appBinFile = Join(pAppBinDir, ChopEnd(*a, 4));

				// Includes
				SetS includes = GetAllIncls(appSrcFile);

				// Check Need-to-Build
				bool needToBuild = false;

				// No Binary
				if (!FileExists(appBinFile))
				{
					needToBuild = true;
				}
				// Include-File Modified
				else if (GetFileModTm(includes) > GetFileModTm(appBinFile))
				{
					needToBuild = true;
				}
				// Source-File Modified
				else if (GetFileModTm(appSrcFile) > GetFileModTm(appBinFile))
				{
					needToBuild = true;
				}
				// Object-File Library Modified
				else if (GetFileModTm(pObjLibArc) > GetFileModTm(appBinFile))
				{
					needToBuild = true;
				}
				// Library-File Modified
				else if (GetFileModTm(libFileNames) > GetFileModTm(appBinFile))
				{
					needToBuild = true;
				}

				// Need To Build
				if (needToBuild)
				{
					String cmd = pCompiler;
					cmd += " "    + pCompPreFlags;
					cmd += " "    + appSrcFile;
					cmd += " -o " + appBinFile;
					cmd += " "    + includeFlags;
					cmd += " "    + libraryFlags;
					cmd += " "    + pCompPostFlags;
					cmds.push_back(cmd);

					// Display
					if (display) Display("Building", "Apps", pAppSrcDir);
				}
			}
		}

		// Spawn Application Builds
		Spawn(cmds, pSpawn);
	}


	//////////////////////////
	// Build Unit-Test Apps //
	//////////////////////////

	{
		// Build Commands
		VecS cmds;

		// Unit-Test-Run Script
		String unitScript = GetVal("UnitTestScript");
		std::ofstream unitStream(unitScript.c_str());

		if (!unitStream)
		{
			std::cerr << "Unable to create unit-test script: " << unitScript << std::endl;
			exit(1);
		}

		// Unit-Test Descriptions
		VecVecS unitDescs = GetValsM("UnitTestDir");

		// For Each Unit-Test Description
		for (VecVecS::iterator u = unitDescs.begin(); u != unitDescs.end(); ++u)
		{
			First display;

			// Unit-Test Description
			VecS unitDesc = *u;

			// Validate Description
			if (unitDesc.size() != 3 || unitDesc[1] != "=>")
			{
				std::cerr << "Bad format in Recipe for UnitTestDir, must be of the form 'unitDir => binDir' not " << Concat(unitDesc) << std::endl;
				exit(1);
			}

			// Unit-Test Directories
			String pUnitSrcDir = unitDesc[0];
			String pUnitBinDir = unitDesc[2];
			MkDir(pUnitBinDir);

			// Unit-Test Source Files
			VecS unitSrcFiles = ListFiles(pUnitSrcDir);

			// For Each Unit-Test Source File
			for (VecS::iterator a = unitSrcFiles.begin(); a != unitSrcFiles.end(); ++a)
			{
				// Only C++ Files
				if (!EndsWith(*a, ".cpp"))
				{
					continue;
				}

				// Unit-Test Source File
				String unitSrcFile = Join(pUnitSrcDir, *a);

				// Unit-Test Binary File
				String unitBinFile = Join(pUnitBinDir, ChopEnd(*a, 4));

				// Add to Unit-Test-Run Script
				unitStream << "./" << unitBinFile << std::endl;

				// Includes
				SetS includes = GetAllIncls(unitSrcFile);

				// Check Need-to-Build
				bool needToBuild = false;

				// No Binary
				if (!FileExists(unitBinFile))
				{
					needToBuild = true;
				}
				// Include Modified
				else if (GetFileModTm(includes) > GetFileModTm(unitBinFile))
				{
					needToBuild = true;
				}
				// Source Modified
				else if (GetFileModTm(unitSrcFile) > GetFileModTm(unitBinFile))
				{
					needToBuild = true;
				}
				// Object Library Modified
				else if (GetFileModTm(pObjLibArc) > GetFileModTm(unitBinFile))
				{
					needToBuild = true;
				}
				// Library-File Modified
				else if (GetFileModTm(libFileNames) > GetFileModTm(unitBinFile))
				{
					needToBuild = true;
				}

				// Need To Build
				if (needToBuild)
				{
					String cmd = pCompiler;
					cmd += " "    + pCompPreFlags;
					cmd += " "    + unitSrcFile;
					cmd += " -o " + unitBinFile;
					cmd += " "    + includeFlags;
					cmd += " "    + libraryFlags;
					cmd += " "    + pCompPostFlags;
					cmds.push_back(cmd);

					// Display
					if (display) Display("Building", "Unit-Tests", pUnitSrcDir);
				}
			}
		}

		// Close Unit-Test-Run Script
		unitStream.close();

		// Spawn Unit-Test Builds
		Spawn(cmds, pSpawn);

		// Run Unit-Tests
		System("chmod u+x " + unitScript);
		System("./" + unitScript);
	}


	return 0;
}






///////////////////////////
// Helper Implementation //
///////////////////////////

// Max
int Max(int a, int b)
{
	return (a > b) ? a : b;
}

// Join Paths
String Join(const String& a, const String& b)
{
	return a + "/" + b;
}

// Get Directory
String GetDir(const String& path)
{
	int final = -1;
	int i = 0;
	while (i < path.size())
	{
		if (path[i] == '/')
		{
			final = i;
		}

		i++;
	}

	if (final == -1)
	{
		return "./";
	}

	return path.substr(0, final);
}

// Join Paths
String Join(const String& a, const String& b, const String& c)
{
	return a + "/" + b + "/" + c;
}

// Concatenate VecS
String Concat(const VecS& vecS)
{
	String result;

	for (VecS::const_iterator s = vecS.begin(); s != vecS.end(); ++s)
	{
		if (s != vecS.begin())
		{
			result += " ";
		}

		result += *s;
	}

	return result;
}

// Concatenate SetS
String Concat(const SetS& setS)
{
	String result;

	for (SetS::const_iterator s = setS.begin(); s != setS.end(); ++s)
	{
		if (s != setS.begin())
		{
			result += " ";
		}

		result += *s;
	}

	return result;
}

// Split
VecS Split(const String& line)
{
	VecS result;
	std::stringstream stream(line);

	String token;
	while (stream >> token)
	{
		result.push_back(token);
	}

	return result;
}

// Display SetS
String ToStr(const SetS& setS)
{
	String result = "Set:";

	for (SetS::const_iterator s = setS.begin(); s != setS.end(); ++s)
	{
		result += " " + *s;
	}

	return result;
}

// Ends-With
bool EndsWith(const String& str, const String& ending)
{
	if (str.size() >= ending.size())
	{
		if (str.substr(str.size() - ending.size(), ending.size()) == ending)
		{
			return true;
		}
	}

	return false;
}

// File-Exists
bool FileExists(const String& path)
{
	struct stat s;

	if (stat(path.c_str(), &s) == 0)
	{
		if (s.st_mode & S_IFREG)
		{
			return true;
		}
	}

	return false;
}

// File Modification Date
int GetFileModTm(const String& filename)
{
	if (FileExists(filename))
	{
		struct stat s;
		if (stat(filename.c_str(), &s) == 0)
		{
			return s.st_mtime;
		}
	}

	return 0;
}

// File Modification Date
int GetFileModTm(const SetS& filenames)
{
	int result = 0;
	for (SetS::const_iterator f = filenames.begin(); f != filenames.end(); ++f)
	{
		int modTime = GetFileModTm(*f);
		if (modTime > result)
		{
			result = modTime;
		}
	}
	return result;
}

// Is Option On
bool IsOn(const String& key)
{
	for (int a = 0; a < args.size(); a++)
	{
		if (args[a] == "-" + key)
		{
			return true;
		}
	}

	return false;
}

// Has Option
bool HasOpt(const String& key)
{
	for (int a = 0; a < args.size(); a++)
	{
		String arg = args[a];
		if (arg.size() > 1 + key.size() + 1)
		{
			// Find -key=Value
			if (arg.substr(0, 1 + key.size() + 1) == "-" + key + "=")
			{
				return true;
			}
		}
	}

	return false;
}

// Get Option
String GetOpt(const String& key)
{
	for (int a = 0; a < args.size(); a++)
	{
		String arg = args[a];
		if (arg.size() > 1 + key.size() + 1)
		{
			// Find -key=Value
			if (arg.substr(0, 1 + key.size() + 1) == "-" + key + "=")
			{
				return arg.substr(1 + key.size() + 1, arg.size() - 1 - key.size() - 1);
			}
		}
	}

	std::cerr << "Missing Option: " << key << std::endl;
	exit(1);
}

// Get Value from Recipe
String GetVal(const String& key)
{
	for (int i = 0; i < lines.size(); i++)
	{
		const VecS& tokens = lines[i];

		if (tokens[0] == key)
		{
			return tokens[1];
		}
	}

	std::cerr << "Can't find key in recipe: " << key << std::endl;
	exit(1);
}

// Get Values from Recipe
VecS GetVals(const String& key)
{
	VecS result;

	for (int i = 0; i < lines.size(); i++)
	{
		const VecS& tokens = lines[i];

		if (tokens[0] == key)
		{
			for (int r = 1; r < tokens.size(); r++)
			{
				result.push_back(tokens[r]);
			}
		}
	}

	if (result.empty())
	{
		std::cerr << "Can't find key in recipe: " << key << std::endl;
		exit(1);
	}

	return result;
}

// Get Multiple Values from Recipe
VecVecS GetValsM(const String& key)
{
	VecVecS result;

	for (int i = 0; i < lines.size(); i++)
	{
		const VecS& tokens = lines[i];

		if (tokens[0] == key)
		{
			VecS match;
			for (int r = 1; r < tokens.size(); r++)
			{
				match.push_back(tokens[r]);
			}
			result.push_back(match);
		}
	}

	return result;
}


// Get Set of Included Files from a File
SetS GetIncls(const String& file)
{
	SetS result;
	std::ifstream stream(file.c_str());

	if (!stream)
	{
		return result;
	}

	String line;
	while (getline(stream, line))
	{
		if (line.substr(0, 8) == "#include")
		{
			VecS tokens = Split(line);
			if (tokens.size() >= 2)
			{
				String include = tokens[1];
				if (include.size() > 2)
				{
					include = include.substr(1, include.size() - 2);

					// Find Exact File (Search Include Directories)
					for (VecS::iterator i = inclDirs.begin(); i != inclDirs.end(); ++i)
					{
						String candidate = Join(*i, include);
						if (FileExists(candidate))
						{
							result.insert(candidate);
							break;
						}
					}
				}
			}
		}
	}

	return result;
}

SetS GetAllIncls(const String& file)
{
	// Immediate Includes
	SetS inclSet = GetIncls(file);

	// Ancestral Iteration
	while (true)
	{
		// All Includes
		SetS nextSet = inclSet;

		// For Each Include
		for (SetS::iterator i = inclSet.begin(); i != inclSet.end(); ++i)
		{
			// Add Ancestors
			SetS ancestors = GetIncls(*i);
			for (SetS::iterator a = ancestors.begin(); a != ancestors.end(); ++a)
			{
				nextSet.insert(*a);
			}
		}

		// No Additional Found
		if (nextSet.size() == inclSet.size())
		{
			break;
		}
			
		inclSet = nextSet;
	}
		
	return inclSet;
}

// Make-Directory
void MkDir(const String& dir)
{
	String cmd = "mkdir -p " + dir;
	int rc = system(cmd.c_str());
}

// Chop-Ending
String ChopEnd(const String& str, int end)
{
	if ((int)str.size() <= end)
	{
		return "";
	}

	return str.substr(0, str.size() - end);
}

// Spawn Set of Commands
void Spawn(const VecS& cmds, int nSpawn)
{
	int c = 0;
	int alive = 0;
	MapIS pids;

	while (c < cmds.size())
	{
		// At Spawn Capacity
		while (alive >= nSpawn)
		{
			int status;

			// Wait for Process to Finish
			int pid = wait(&status);

			// Known Process
			if (pids.find(pid) != pids.end())
			{
				// Abnormal Termination
				if (!WIFEXITED(status))
				{
					std::cerr << Prefix
							  << FgRed() << "Execution Failed: " << FgOff()
							  << FgYlw() << pids[pid] << FgOff() << std::endl;
					exit(1);
				}

				pids.erase(pid);
				alive--;
			}
		}

		// Next Command
		const String& cmd = cmds[c];

		// Fork and Execute
		int pid = fork();

		// Error
		if (pid < 0)
		{
			std::cerr << "Failed to fork()" << std::endl;
			exit(1);
		}
		// Child
		else if (pid == 0)
		{
			std::cout << Prefix << FgGrn() << "Executing: " << FgOff() << cmd << std::endl;
			system(cmd.c_str());
			exit(0);
		}
		// Parent
		else
		{
			pids[pid] = cmd;
			alive++;
		}

		// Next Command
		c++;
	}

	// Finish Remaining
	while (alive > 0)
	{
		int status;

		// Wait for Process to Finish
		int pid = wait(&status);

		// Known Process
		if (pids.find(pid) != pids.end())
		{
			// Abnormal Termination
			if (!WIFEXITED(status))
			{
				std::cerr << Prefix << FgRed() << "Execution Failed: " << FgOff() << pids[pid] << std::endl;
				exit(1);
			}

			pids.erase(pid);
			alive--;
		}
	}
}

// List Files in a Directory
VecS ListFiles(const String& dir)
{
	VecS result;

	// Open Directory
	DIR* d = opendir(dir.c_str());
	dirent *ent;

	// Directory Does Not Exist
	if (!d)
	{
		return result;
	}

	// For Each Entry
	while (ent = readdir(d))
	{
		String filename = ent->d_name;
		String fullPath = Join(dir, filename);

		// Empty Filename
		if (filename.empty())
		{
			continue;
		}

		// Exclude Self
		if (filename[0] == '.')
		{
			continue;
		}

		// Get File Info
		// Get File Info
		struct stat st;
		if (stat(fullPath.c_str(), &st) == -1)
		{
			continue;
		}

		// Not A File
		if (st.st_mode & S_IFDIR == 0)
		{
			continue;
		}

		// Add File
		result.push_back(filename);
	}

	closedir(d);
	return result;
}

// System Call
int System(const String& cmd)
{
	return system(cmd.c_str());
}

// Get All Library Files
SetS GetLibFiles()
{
	SetS result;

	VecS libPaths = GetVals("LibraryDirs");
	VecS libNames = GetVals("Libraries");

	for (VecS::iterator p = libPaths.begin(); p != libPaths.end(); ++p)
	{
		for (VecS::iterator n = libNames.begin(); n != libNames.end(); ++n)
		{
			String sharedObj = Join(*p, "lib" + *n + ".so");
			String staticArc = Join(*p, "lib" + *n + ".a");

			if (FileExists(sharedObj))
			{
				result.insert(sharedObj);
			}

			if (FileExists(staticArc))
			{
				result.insert(staticArc);
			}
		}
	}

	return result;
}

String FgOn(int color)
{
	char buffer[20];
	sprintf(buffer, "%c[38;5;%dm", 0x1B, color);
	return buffer;
}

String FgOff()
{
	char buffer[20];
	sprintf(buffer, "%c[%dm", 0x1B, 0);
	return buffer;
}

String FgRed() { return FgOn(160); }
String FgBlu() { return FgOn(129); }
String FgGrn() { return FgOn(150); }
String FgYlw() { return FgOn(190); }
String FgSky() { return FgOn(153); }
String FgOrg() { return FgOn(214); }

String Dashes(int n)
{
	return String(n, '-');
}

String Star()
{
	return FgSky() + "* " + FgOff();
}

void Display(const String& label, const String& target, const String& detail)
{
	std::cout << Prefix
			  << FgBlu() << label  << ": " << FgOff()
			  << FgYlw() << target << " (" << FgOff()
			  << FgGrn() << detail         << FgOff()
			  << FgYlw()           << ")"  << FgOff()
			  << std::endl;
}


