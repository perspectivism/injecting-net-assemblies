using FirstFloor.ModernUI.Windows.Controls;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Windows;

namespace Inject
{
    #region Helper Classes

    /// <summary>
    /// Helper class for keeping track of running processes.
    /// </summary>
    public class ProcessItem
    {
        public string DisplayName { get; set; }
        public string Name { get; set; }
        public int Id { get; set; }
        public bool Is64Bit { get; set; }
    }

    /// <summary>
    /// Helper class for storing info about managed methods.
    /// </summary>
    public class MethodItem
    {
        public string TypeName { get; set; }
        public string Name { get; set; }
        public string ParameterName { get; set; }
    }

    #endregion

    /// <summary>
    /// Managed wrapper that provides an advanced interface for the Inject.exe app.
    /// </summary>
    public class InjectWrapper : DependencyObject
    {
#if DEBUG
        private const string CONFIGURATION = "Debug";
#else
        private const string CONFIGURATION = "Release";
#endif

        [DllImport("kernel32.dll", SetLastError = true, CallingConvention = CallingConvention.Winapi)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool IsWow64Process([In] IntPtr process, [Out] out bool wow64Process);

        #region Dependency Properties

        public static readonly DependencyProperty ProcessListProperty =
            DependencyProperty.Register("ProcessList", typeof(List<ProcessItem>), typeof(InjectWrapper), new PropertyMetadata(null));

        public static readonly DependencyProperty ProcessIdProperty =
            DependencyProperty.Register("ProcessId", typeof(int), typeof(InjectWrapper), new PropertyMetadata(0));

        public static readonly DependencyProperty ManagedFilenameProperty =
            DependencyProperty.Register("ManagedFilename", typeof(String), typeof(InjectWrapper), new PropertyMetadata(String.Empty));

        public static readonly DependencyProperty InjectableMethodsProperty =
            DependencyProperty.Register("InjectableMethods", typeof(List<MethodItem>), typeof(InjectWrapper), null);

        public static readonly DependencyProperty MethodNameProperty =
            DependencyProperty.Register("MethodName", typeof(String), typeof(InjectWrapper), new PropertyMetadata(String.Empty));

        // list of running processes
        public List<ProcessItem> ProcessList
        {
            get { return (List<ProcessItem>)GetValue(ProcessListProperty); }
            set { SetValue(ProcessListProperty, value); }
        }

        // currently selected process
        public int ProcessId
        {
            get { return (int)GetValue(ProcessIdProperty); }
            set { SetValue(ProcessIdProperty, value); }
        }

        // managed assembly filename
        public String ManagedFilename
        {
            get { return (String)GetValue(ManagedFilenameProperty); }
            set { SetValue(ManagedFilenameProperty, value); }
        }

        // valid methods that can be injected
        public List<MethodItem> InjectableMethods
        {
            get { return (List<MethodItem>)GetValue(InjectableMethodsProperty); }
            set { SetValue(InjectableMethodsProperty, value); }
        }

        // currently selected method to inject
        public String MethodName
        {
            get { return (String)GetValue(MethodNameProperty); }
            set { SetValue(MethodNameProperty, value); }
        }

        // the optional argument to pass into the injected method
        public String Argument { get; set; }

        #endregion

        // Constructor
        public InjectWrapper()
        {
            RefreshProcessList();
        }

        #region Public Methods

        // performs the injection
        public void Inject()
        {
            if (!IsInjectCmdValid())
                return;

            Process.Start(GetInjectStartInfo());
        }

        // copies the injection command to the clipboard
        public void CopyCmdLineToClipboard()
        {
            if (!IsInjectCmdValid())
                return;

            ProcessStartInfo info = GetInjectStartInfo();
            Clipboard.SetText(String.Format("\"{0}\" {1}", info.FileName, info.Arguments));
        }

        // refresh the list of running processes
        public void RefreshProcessList()
        {
            // save current process id
            int pid = ProcessId;

            // distinguish if debugger is attached to avoid slowness due to first-chance exceptions

            if (Debugger.IsAttached)
            {
                // without bitness check
                ProcessList = (from p in Process.GetProcesses().ToList().OrderBy(x => x.ProcessName)
                               select new ProcessItem
                               {
                                   DisplayName = string.Format("{0,5}\t{1}", p.Id, p.ProcessName),
                                   Name = p.ProcessName,
                                   Id = p.Id
                               }).ToList();
            }
            else
            {
                // with bitness check
                ProcessList = (from p in Process.GetProcesses().ToList().OrderBy(x => x.ProcessName)
                               select new ProcessItem
                               {
                                   DisplayName = string.Format("{0,5}\t{1}\t{2}", p.Id, GetPlatform(p.Id), p.ProcessName),
                                   Name = p.ProcessName,
                                   Id = p.Id,
                                   Is64Bit = IsWow64Process(p.Id)
                               }).ToList();
            }

            // set to -1 to hint to all ux controls bound to ProcessId to maintain the currently selected pid if pid value has not changed
            ProcessId = -1;

            // is selected process still running?
            bool exists = ProcessList.Exists(p => p.Id == pid);

            // set the process id
            ProcessId = exists ? pid : 0;
        }


        // let the user browse for a .net assembly and then parse all injectable methods
        public void BrowseAndParseAssembly()
        {
            // build file chooser
            Microsoft.Win32.OpenFileDialog dialog = new Microsoft.Win32.OpenFileDialog();
            dialog.Filter = "All Files (*.*)|*.*|Exe Files (*.exe)|*.exe|Dll Files (*.dll)|*.dll;";
            bool? result = dialog.ShowDialog();

            if (result == false || string.IsNullOrEmpty(dialog.FileName))
                return;

            // get filename
            ManagedFilename = dialog.FileName;

            // extract methods with valid injectable signatures
            ExtractInjectableMethods();
        }

        // display a message box describing what valid injection methods look like
        public void ShowValidSignature()
        {
            ModernDialog.ShowMessage("Valid methods must have the following signature:\n\n\t[color=Blue]static int[/color] pwzMethodName ([color=#FF2B91AF]String[/color] pwzArgument)", "Valid Signature", MessageBoxButton.OK);
        }

        #endregion

        #region Private Methods

        // check if process is 32 or 64 bit
        private static bool IsWow64Process(int id)
        {
            if (!Environment.Is64BitOperatingSystem)
                return true;

            IntPtr processHandle;
            bool retVal;

            try
            {
                processHandle = Process.GetProcessById(id).Handle;
            }
            catch
            {
                return false; // access is denied to the process
            }

            return IsWow64Process(processHandle, out retVal) && retVal;
        }

        // get platform string
        private string GetPlatform(int id)
        {
            return IsWow64Process(id) ? "x86" : "x64";
        }

        // get platform string
        private string GetPlatform()
        {
            return IsWow64Process(ProcessId) ? "x86" : "x64";
        }

        // iterate the given assembly file and extract methods which match injectable signatures
        // a valid signature is one of the form: 
        //
        //      static int EntryPoint(String pwzArgument)
        //
        private void ExtractInjectableMethods()
        {
            try
            {
                // open assembly
                Assembly asm = Assembly.LoadFile(ManagedFilename);

                // get valid methods
                InjectableMethods = (from c in asm.GetTypes()
                                     from m in c.GetMethods(BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic)
                                     where m.ReturnType == typeof(int) &&
                                         m.GetParameters().Length == 1 &&
                                         m.GetParameters().First().ParameterType == typeof(string)
                                     select new MethodItem
                                     {
                                         Name = m.Name,
                                         ParameterName = m.GetParameters().First().Name,
                                         TypeName = m.ReflectedType.FullName
                                     }).ToList();

                if (InjectableMethods.Count() == 0) // no methods found
                {
                    // reset method name
                    MethodName = String.Empty;

                    // show not found message
                    ModernDialog.ShowMessage("Could not find any methods in this assembly that can be used for injection.", "Warning", MessageBoxButton.OK);
                }
                else // found some methods
                {
                    // default combobox to first item
                    MethodName = InjectableMethods.First().Name;

                    // show result message
                    ModernDialog.ShowMessage(string.Format("Found [b]{0} method{1}[/b] that can be injected.", InjectableMethods.Count(), InjectableMethods.Count() == 1 ? string.Empty : "s"), "Info", MessageBoxButton.OK);
                }
            }
            catch
            {
                MethodName = String.Empty;
                ManagedFilename = String.Empty;
                InjectableMethods = null;
                ModernDialog.ShowMessage("An error occurred when reading the file. Are you sure this is a valid .NET assembly?", "Error", MessageBoxButton.OK);
            }
        }

        private void BuildInjectApp()
        {
            // common parameters

            const string msbuild = @"C:\Windows\Microsoft.NET\Framework\v4.0.30319\MSBuild.exe";
            ProcessStartInfo info = new ProcessStartInfo
            {
                CreateNoWindow = true,
                UseShellExecute = false,
                FileName = msbuild
            };

            string startupDir = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
            string outDir = string.Format("\"{0}\\{1}\"", startupDir, GetPlatform());

            // build Inject project
            string args = string.Format("\"{0}\" /p:VisualStudioVersion=11.0 /p:Platform={1} /p:Configuration={2} /p:OutDir={3}", @".\..\..\..\Inject\Inject.vcxproj", GetPlatform(), CONFIGURATION, outDir);
            info.Arguments = args;
            Process.Start(info).WaitForExit();

            // build Bootstrap project
            args = string.Format("\"{0}\" /p:VisualStudioVersion=11.0 /p:Platform={1} /p:Configuration={2} /p:OutDir={3}", @".\..\..\..\Bootstrap\Bootstrap.vcxproj", GetPlatform(), CONFIGURATION, outDir);
            info.Arguments = args;
            Process.Start(info).WaitForExit();
        }

        // validates that the c++ inject.exe command can be correctly invoked
        private bool IsInjectCmdValid()
        {
            // validate paramaters
            if (InjectableMethods == null || MethodName == null)
            {
                ModernDialog.ShowMessage("Please fill in all required fields and try again.", "Warning", MessageBoxButton.OK);
                return false;
            }
            // validate count
            else if (InjectableMethods.Count() == 0)
            {
                ModernDialog.ShowMessage("The chosen assembly file does not contain any valid, injectable methods.", "Warning", MessageBoxButton.OK);
                return false;
            }

            // validate inject and bootstrap modules exist where expected
            string injectFile = GetInjectStartInfo().FileName;
            string bootstrapFile = Path.Combine(Path.GetDirectoryName(injectFile), "Bootstrap.dll");
            if (!File.Exists(injectFile) || !File.Exists(bootstrapFile))
            {
                // ask user if they want to build it
                bool? result = ModernDialog.ShowMessage("Not all of the dependencies for [inject.exe] were found. Would you like to build them now?", "Warning", MessageBoxButton.YesNo);
                if (result == false)
                    return false; // guess not

                // build the inject app
                BuildInjectApp();
                result = ModernDialog.ShowMessage("All dependencies have been built. Would you like to continue?", "Info", MessageBoxButton.YesNo);
                if (result == false)
                    return false; // cancel requested operation
            }

            return true;
        }

        // get the inject cmd process startup information
        private ProcessStartInfo GetInjectStartInfo()
        {
            // get startup location
            string startupDir = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);

            // get x86 or x64 inject path
            string filename = Path.Combine(startupDir, GetPlatform(), "Inject.exe");

            // build args
            MethodItem item = InjectableMethods.Where(m => m.Name == MethodName).First();
            string args = String.Format("-m {0} -i \"{1}\" -l {2} -a \"{3}\" -n {4}", item.Name, ManagedFilename, item.TypeName, Argument, ProcessId);

            // create the process info
            ProcessStartInfo info = new ProcessStartInfo
            {
                CreateNoWindow = true,
                UseShellExecute = false,
                FileName = filename,
                Arguments = args
            };

            return info;
        }

        #endregion
    }
}
