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

namespace BarcodeScanner
{
    public partial class Form1 : Form
    {
        private FilterInfoCollection CaptureDevice;
        private VideoCaptureDevice FinalFrame;

        public Form1()
        {
            InitializeComponent();
        }

        private void cboCamera_SelectedIndexChanged(object sender, EventArgs e)
        {

        }

        private void Form1_Load(object sender, EventArgs e)
        {
            CaptureDevice = new FilterInfoCollection(FilterCategory.VideoInputDevice);
            foreach (FilterInfo Device in CaptureDevice)
            {
                cboCamera.Items.Add(Device.Name);
            }

            cboCamera.SelectedIndex = 0;
            FinalFrame = new VideoCaptureDevice();
        }

        bool isAttached = false;
        private void btnOpen_Click(object sender, EventArgs e)
        {
            if (!isAttached)
            {
                FinalFrame = new VideoCaptureDevice(CaptureDevice[cboCamera.SelectedIndex].MonikerString);
                FinalFrame.NewFrame += FinalFrame_NewFrame;
                isAttached = true;
            }
            FinalFrame.Start();
            timer1.Enabled = false;
            btnShow.Enabled = true;
            btnShow.Focus();
        }

        private void FinalFrame_NewFrame(object sender, NewFrameEventArgs eventArgs)
        {
            pictureBox1.Image = (Bitmap)eventArgs.Frame.Clone();
        }

        private void btnShow_Click(object sender, EventArgs e)
        {
            timer1.Enabled = true;
            timer1.Start();
            btnShow.Enabled = false;
        }

        private void timer1_Tick(object sender, EventArgs e)
        {
            BarcodeReader reader = new BarcodeReader();
            Result result = reader.Decode((Bitmap)pictureBox1.Image);
            try
            {
                string decoded = result.ToString().Trim();
                if (decoded != "")
                {
                    timer1.Stop();
                    if (MessageBox.Show(decoded + Environment.NewLine + "ต้องการเขียน NFC Tag ใช่หรือไม่ ?", "QR code", MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes)
                    {
                        // Write
                        
                    }

                }
                btnShow.Enabled = true;
            }
            catch (Exception ex)
            {

            }            
        }

        private void Form1_FormClosing(object sender, FormClosingEventArgs e)
        {
            timer1.Enabled = false;
            if (FinalFrame.IsRunning == true)
            {
                FinalFrame.Stop();
            }
        }
    }
}
