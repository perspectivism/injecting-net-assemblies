using System.Windows;
using System.Windows.Controls;

namespace Inject.Pages
{    
    /// <summary>
    /// GUI to ease the task of injecting managed code.
    /// </summary>
    public partial class Home : UserControl
    {
        // wrapper property
        public InjectWrapper Wrapper { get; set; }

        // constructor
        public Home()
        {
            Wrapper = new InjectWrapper();
            InitializeComponent();
        }

        // refresh the list of running processes
        private void RefreshProcessList_Click(object sender, RoutedEventArgs e)
        {
            Wrapper.RefreshProcessList();
        }

        // pick managed assembly and extract valid methods that can be injected
        private void BrowseAssembly_Click(object sender, RoutedEventArgs e)
        {
            Wrapper.BrowseAndParseAssembly();
        }

        // display valid signature message box
        private void ValidSignature_Click(object sender, RoutedEventArgs e)
        {
            Wrapper.ShowValidSignature();
        }

        // inject the managed assembly
        private void Inject_Click(object sender, RoutedEventArgs e)
        {
            Wrapper.Inject();
        }

        // copy the injection command to the clipboard
        private void CopyCmdLineToClipboard_Click(object sender, RoutedEventArgs e)
        {
            Wrapper.CopyCmdLineToClipboard();
        } 
    }
}
