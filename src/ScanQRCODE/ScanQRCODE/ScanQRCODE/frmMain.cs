using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO;
using AForge;
using AForge.Video;
using AForge.Video.DirectShow;
using ZXing;
using ZXing.Aztec;

namespace ScanQRCODE
{
    public partial class frmMain : Form
    {
        public frmMain()
        {
            InitializeComponent();
        }

        VideoCaptureDevice FinalFrame;
        FilterInfoCollection CaptureDevice;
        Bitmap img;

        private void frmMain_Load(object sender, EventArgs e)
        {
             CaptureDevice = new FilterInfoCollection(FilterCategory.VideoInputDevice);
            foreach (FilterInfo Device in CaptureDevice)
            {
                cboDriveCamera.Items.Add(Device.Name);
            }
            cboDriveCamera.SelectedIndex = 0;
             FinalFrame = new VideoCaptureDevice();
        }

        private void btnStart_Click(object sender, EventArgs e)
        {
              FinalFrame = new VideoCaptureDevice(CaptureDevice[cboDriveCamera.SelectedIndex].MonikerString);
              FinalFrame.NewFrame += new NewFrameEventHandler(FinalFrame_NewFrame);
              FinalFrame.Start();

            timer1.Enabled = true;
            timer1.Start();

            //  pictureBox1.Image = Image.FromFile("d:\\444.png");
            //   img = (Bitmap) Image.FromFile("d:\\444.png");
        }

        private void FinalFrame_NewFrame(object sender, NewFrameEventArgs eventArgs)
        {
            pictureBox1.Image = (Bitmap)eventArgs.Frame.Clone();
           img = (Bitmap)eventArgs.Frame.Clone();
        }

        private void timer1_Tick(object sender, EventArgs e)
        {

            try
            {
                BarcodeReader Reader = new BarcodeReader();
                //  img = (Bitmap)pictureBox1.Image;
                var result = Reader.Decode(img);

                string decoded = result.ToString().Trim();
                if (decoded != "")
                {
                    timer1.Stop();
                    txtOutpot.Text =   decoded;
                    MessageBox.Show("OK","Output :");
              //      Form2 form = new Form2();
              //      form.Show();
              //      this.Hide();

                }
            }
            catch (Exception ex)
            {
                txtOutpot.Text = "";
            }
        }

        private void btnStop_Click(object sender, EventArgs e)
        {
            timer1.Stop();
            timer1.Enabled = false;

            if (FinalFrame.IsRunning == true)
            {
                FinalFrame.Stop();
            }
            pictureBox1.Image = null;
        }

        private void frmMain_FormClosing(object sender, FormClosingEventArgs e)
        {
            if (FinalFrame.IsRunning == true)
            {
                FinalFrame.Stop();
            }
        }
    }
}
