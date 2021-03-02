#!/usr/bin/env python
#-*- coding: ascii -*-

import os, sys, multiprocessing, subprocess, shutil, platform

def LogError(message):
	print("[E] %s" % message)
	sys.stdout.flush()
	if sys.platform.startswith("win"):
		pause_cmd = "pause"
	else:
		pause_cmd = "read"
	subprocess.call(pause_cmd, shell = True)
	sys.exit(1)

def LogInfo(message):
	print("[I] %s" % message)
	sys.stdout.flush()

def LogWarning(message):
	print("[W] %s" % message)
	sys.stdout.flush()

class CompilerInfo:
	def __init__(self, build_info, arch, gen_name, compiler_root, vcvarsall_path = "", vcvarsall_options = ""):
		self.arch = arch
		self.generator = gen_name
		self.compiler_root = compiler_root
		self.vcvarsall_path = vcvarsall_path
		self.vcvarsall_options = vcvarsall_options

class BuildInfo:
	@classmethod
	def FromArgv(BuildInfo, argv, base = 0):
		project = None
		compiler = None
		archs = None
		cfg = None

		argc = len(argv)
		if argc > base + 1:
			project = argv[base + 1]
		if argc > base + 2:
			compiler = argv[base + 2]
		if argc > base + 3:
			archs = (argv[base + 3], )
		if argc > base + 4:
			cfg = (argv[base + 4], )

		return BuildInfo(project, compiler, archs, cfg)

	def __init__(self, project, compiler, archs, cfg):
		host_platform = sys.platform
		if host_platform.startswith("win"):
			host_platform = "win"
		target_platform = host_platform

		self.host_platform = host_platform
		self.target_platform = target_platform

		self.host_arch = platform.machine()
		if (self.host_arch == "AMD64") or (self.host_arch == "x86_64"):
			self.host_arch = "x64"
		elif (self.host_arch == "ARM64"):
			self.host_arch = "arm64"
		else:
			LogError("Unknown host architecture %s.\n" % self.host_arch)

		if self.host_platform == "win":
			self.sep = "\r\n"
		else:
			self.sep = "\n"

		self.is_windows = False

		if "win" == target_platform:
			self.is_windows = True

		if compiler is None:
			if project is None:
				if self.is_windows:
					program_files_folder = self.FindProgramFilesFolder()

					if self.FindVS2019Folder(program_files_folder) is not None:
						project = "vs2019"
						compiler = "vc142"
				else:
					LogError("Unsupported target platform %s.\n" % target_platform)
			else:
				if project == "vs2019":
					compiler = "vc142"

		if self.is_windows:
			program_files_folder = self.FindProgramFilesFolder()

			if "vc142" == compiler:
				if project == "vs2019":
					try_folder = self.FindVS2019Folder(program_files_folder)
					if try_folder is not None:
						compiler_root = try_folder
						vcvarsall_path = "VCVARSALL.BAT"
						vcvarsall_options = ""
					else:
						LogError("Could NOT find vc142 compiler toolset for VS2019.\n")
				else:
					LogError("Could NOT find vc142 compiler.\n")
			elif "clangcl" == compiler:
				if project == "vs2019":
					try_folder = self.FindVS2019Folder(program_files_folder)
					if try_folder is not None:
						compiler_root = try_folder
						vcvarsall_path = "VCVARSALL.BAT"
						vcvarsall_options = ""
					else:
						LogError("Could NOT find clang-cl compiler toolset for VS2019.\n")
				else:
					LogError("Could NOT find clang-cl compiler.\n")
		else:
			compiler_root = ""

		if project is None:
			if "vc142" == compiler:
				project = "vs2019"
			else:
				project = "make"

		if archs is None:
			archs = ("x64", )

		if cfg is None:
			cfg = ("Debug", "RelWithDebInfo")

		compilers = []
		if "vs2019" == project:
			self.vs_version = 16
			if "vc142" == compiler:
				compiler_name = "vc"
				compiler_version = 142
			elif "clangcl" == compiler:
				compiler_name = "clangcl"
				compiler_version = self.RetrieveClangVersion(compiler_root + "../../Tools/Llvm/bin/")
			else:
				LogError("Wrong combination of project %s and compiler %s.\n" % (project, compiler))
			for arch in archs:
				compilers.append(CompilerInfo(self, arch, "Visual Studio 16", compiler_root, vcvarsall_path, vcvarsall_options))
		else:
			compiler_name = ""
			compiler_version = 0
			LogError("Unsupported project type %s.\n" % project)

		if project.startswith("vs"):
			self.proj_ext_name = "vcxproj"

		self.project_type = project
		self.compiler_name = compiler_name
		self.compiler_version = compiler_version
		self.compilers = compilers
		self.archs = archs
		self.cfg = cfg

		self.jobs = multiprocessing.cpu_count()

		self.DisplayInfo()

	def MSBuildAddBuildCommand(self, batch_cmd, sln_name, proj_name, config, arch):
		batch_cmd.AddCommand('@SET VisualStudioVersion=%d.0' % self.vs_version)
		if len(proj_name) != 0:
			file_name = "%s.%s" % (proj_name, self.proj_ext_name)
		else:
			file_name = "%s.sln" % sln_name
		batch_cmd.AddCommand('@MSBuild %s /nologo /m:%d /v:m /p:Configuration=%s,Platform=%s' % (file_name, self.jobs, config, arch))
		batch_cmd.AddCommand('@if ERRORLEVEL 1 exit /B 1')

	def MakeAddBuildCommand(self, batch_cmd, make_name, target):
		make_options = "-j%d" % self.jobs
		if target != "ALL_BUILD":
			make_options += " %s" % target
		if "win" == self.host_platform:
			batch_cmd.AddCommand("@%s %s" % (make_name, make_options))
			batch_cmd.AddCommand('@if ERRORLEVEL 1 exit /B 1')
		else:
			batch_cmd.AddCommand("%s %s" % (make_name, make_options))
			batch_cmd.AddCommand('if [ $? -ne 0 ]; then exit 1; fi')

	def FindProgramFilesFolder(self):
		env = os.environ
		if platform.architecture()[0] == '64bit':
			if "ProgramFiles(x86)" in env:
				program_files_folder = env["ProgramFiles(x86)"]
			else:
				program_files_folder = "C:\Program Files (x86)"
		else:
			if "ProgramFiles" in env:
				program_files_folder = env["ProgramFiles"]
			else:
				program_files_folder = "C:\Program Files"
		return program_files_folder

	def FindVS2019Folder(self, program_files_folder):
		return self.FindVS2017PlusFolder(program_files_folder, 16, "2019")

	def FindVS2017PlusFolder(self, program_files_folder, vs_version, vs_name):
		try_vswhere_location = program_files_folder + "\\Microsoft Visual Studio\\Installer\\vswhere.exe"
		if os.path.exists(try_vswhere_location):
			vs_location = subprocess.check_output([try_vswhere_location,
				"-products", "*",
				"-latest",
				"-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
				"-property", "installationPath",
				"-version", "[%d.0,%d.0)" % (vs_version, vs_version + 1),
				"-prerelease"]).decode().split("\r\n")[0]
			try_folder = vs_location + "\\VC\\Auxiliary\\Build\\"
			try_vcvarsall = "VCVARSALL.BAT"
			if os.path.exists(try_folder + try_vcvarsall):
				return try_folder
		else:
			names = ("Preview", vs_name)
			skus = ("Community", "Professional", "Enterprise", "BuildTools")
			for name in names:
				for sku in skus:
					try_folder = program_files_folder + "\\Microsoft Visual Studio\\%s\\%s\\VC\\Auxiliary\\Build\\" % (name, sku)
					try_vcvarsall = "VCVARSALL.BAT"
					if os.path.exists(try_folder + try_vcvarsall):
						return try_folder
		return None

	def DisplayInfo(self):
		print("Build information:")
		print("\tHost platform: %s" % self.host_platform)
		print("\tTarget platform: %s" % self.target_platform)
		print("\tCPU count: %d" % self.jobs)
		print("\tProject type: %s" % self.project_type)
		print("\tCompiler: %s%d" % (self.compiler_name, self.compiler_version))
		archs = ""
		for i in range(0, len(self.compilers)):
			archs += self.compilers[i].arch
			if i != len(self.compilers) - 1:
				archs += ", "
		print("\tArchitectures: %s" % archs)
		cfgs = ""
		for i in range(0, len(self.cfg)):
			cfgs += self.cfg[i]
			if i != len(self.cfg) - 1:
				cfgs += ", "
		print("\tConfigures: %s" % cfgs)
		
		print("")
		sys.stdout.flush()

	def GetBuildDir(self, arch, config = None):
		env = os.environ
		if "BUILD_DIR" in env:
			build_dir_name = env["BUILD_DIR"]
		else:
			build_dir_name = "%s_%s%d_%s_%s" % (self.project_type, self.compiler_name, self.compiler_version, self.target_platform, arch)
			if not (config is None):
				build_dir_name += "-" + config
		return build_dir_name

class BatchCommand:
	def __init__(self, host_platform):
		self.commands_ = []
		self.host_platform_ = host_platform

	def AddCommand(self, cmd):
		self.commands_ += [cmd]

	def Execute(self):
		batch_file = "kge_build."
		if "win" == self.host_platform_:
			batch_file += "bat"
		else:
			batch_file += "sh"
		batch_f = open(batch_file, "w")
		batch_f.writelines([cmd_line + "\n" for cmd_line in self.commands_])
		batch_f.close()
		if "win" == self.host_platform_:
			ret_code = subprocess.call(batch_file, shell = True)
		else:
			subprocess.call("chmod 777 " + batch_file, shell = True)
			ret_code = subprocess.call("./" + batch_file, shell = True)
		os.remove(batch_file)
		return ret_code

def BuildProjects(name, build_path, build_info, compiler_info, project_list, additional_options = ""):
	curdir = os.path.abspath(os.curdir)

	toolset_name = ""
	if build_info.project_type.startswith("vs"):
		toolset_name = "-T"
		if build_info.compiler_name == "clangcl":
			toolset_name += " ClangCL"
		else:
			toolset_name += " v%s," % build_info.compiler_version
			toolset_name += "host=x64"

	if build_info.project_type.startswith("vs"):
		if "x64" == compiler_info.arch:
			vc_option = "amd64"
			vc_arch = "x64"
		else:
			LogError("Unsupported VS architecture.\n")
		if len(compiler_info.vcvarsall_options) > 0:
			vc_option += " %s" % compiler_info.vcvarsall_options

	if build_info.project_type.startswith("vs"):
		additional_options += " -A %s" % vc_arch
		if build_info.compiler_name == "clangcl":
			additional_options += " -DClangCL_Path=\"" + compiler_info.compiler_root + "../../Tools/Llvm/bin/\""

	build_dir = "%s/Build/%s" % (build_path, build_info.GetBuildDir(compiler_info.arch))

	print("Building %s..." % name)
	sys.stdout.flush()

	if not os.path.exists(build_dir):
		os.makedirs(build_dir)

	build_dir = os.path.abspath(build_dir)
	os.chdir(build_dir)

	cmake_cmd = BatchCommand(build_info.host_platform)
	new_path = sys.exec_prefix
	if len(compiler_info.compiler_root) > 0:
		new_path += ";" + compiler_info.compiler_root
	if "win" == build_info.host_platform:
		cmake_cmd.AddCommand('@SET PATH=%s;%%PATH%%' % new_path)
		if build_info.project_type.startswith("vs"):
			cmake_cmd.AddCommand('@CALL "%s%s" %s' % (compiler_info.compiler_root, compiler_info.vcvarsall_path, vc_option))
			cmake_cmd.AddCommand('@CD /d "%s"' % build_dir)
	else:
		cmake_cmd.AddCommand('export PATH=$PATH:%s' % new_path)
	cmake_cmd.AddCommand('cmake -G "%s" %s %s ../../' % (compiler_info.generator, toolset_name, additional_options))
	if cmake_cmd.Execute() != 0:
		LogWarning("Config %s failed, retry 1...\n" % name)
		if cmake_cmd.Execute() != 0:
			LogWarning("Config %s failed, retry 2...\n" % name)
			if cmake_cmd.Execute() != 0:
				LogError("Config %s failed.\n" % name)

	build_cmd = BatchCommand(build_info.host_platform)
	if build_info.project_type.startswith("vs"):
		build_cmd.AddCommand('@CALL "%s%s" %s' % (compiler_info.compiler_root, compiler_info.vcvarsall_path, vc_option))
		build_cmd.AddCommand('@CD /d "%s"' % build_dir)
	for config in build_info.cfg:
		for target in project_list:
			if build_info.project_type.startswith("vs"):
				build_info.MSBuildAddBuildCommand(build_cmd, name, target, config, vc_arch)
	if build_cmd.Execute() != 0:
		LogError("Build %s failed.\n" % name)

	os.chdir(curdir)

	print("")
	sys.stdout.flush()

if __name__ == "__main__":
	build_info = BuildInfo.FromArgv(sys.argv)

	for compiler_info in build_info.compilers:
		BuildProjects("GoldenSun", ".", build_info, compiler_info, ("ALL_BUILD", ))
