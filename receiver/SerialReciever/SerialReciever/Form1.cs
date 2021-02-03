using System;
using System.Windows.Forms;
using System.IO.Ports;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

namespace SerialReciever
{
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();
        }

        static bool isAlive = true;

        static int pos = 0;

        // TODO: Make the COM thing variable to avoid presentation mess-ups.
        static SerialPort Serial;
        // TODO: Check that this whole static thing is the best way to go. Is there a better way.
        static PictureBox Picture;

        static Bitmap bmp;
        static Rectangle bounds;

        Thread receiveThread;

        private void Form1_Load(object sender, EventArgs e)
        {
            Picture = pictureBox;
            Serial = new SerialPort("COM3", 115200);
            Serial.Open();

            // Send some useless data just so I can see if the light on the Arduino lights up.
            Serial.Write(Encoding.ASCII.GetBytes("A"), 0, 1);

            Serial.DiscardInBuffer(); // This is necessary because there is still stuff left in there from the program upload to arduino or something. Really weird. Find out more about it if you can. TODO.

            receiveThread = new Thread(new ThreadStart(receive));
            receiveThread.Start();

            this.FormClosing += closing;
        }

        private void closing(object sender, EventArgs e)
        {
            isAlive = false;
            receiveThread.Join();
        }

        static byte[] DimBuf = new byte[8];
        static byte[] buffer;

        static bool mode = false;

        static int BytesRead = 0;

        static void thing()
        {
            BitmapData bmpData2 = bmp.LockBits(bounds, ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
            Marshal.Copy(buffer, pos - BytesRead, bmpData2.Scan0 + pos - BytesRead, BytesRead);
            bmp.UnlockBits(bmpData2);
            if (pos == buffer.Length)
            {
                Console.WriteLine("Finished receiving data and now showing bitmap.");
                BitmapData bmpData = bmp.LockBits(bounds, ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
                Marshal.Copy(buffer, 0, bmpData.Scan0, buffer.Length);
                bmp.UnlockBits(bmpData);
                pos = 0;
            }
        }

        static void receive()
        {
            while (isAlive)
            {
                if (mode)
                {
                    BytesRead = 0;
                    BytesRead = Serial.Read(buffer, pos, buffer.Length - pos);
                    pos += BytesRead;
                    if (BytesRead != 0)
                    {
                        Console.WriteLine("Received image data.");
                        //Picture.Image = null;           // TODO: This probably isn't necessary.
                        Picture.Invoke(new MethodInvoker(thing));
                        Picture.Image = bmp;
                    }
                    continue;
                }

                // This message isn't really valid when it comes because of the loop and everything.
                // Also Serial.Read blocks when nothing comes which is why it doesn't get spit out over and over.
                Console.WriteLine("Recieved first bytes. Parsing dimensions of image...");

                BytesRead = Serial.Read(DimBuf, pos, 8 - pos);
                pos += BytesRead;

                Console.WriteLine("New pos: " + pos);
                if (pos == 8)
                {
                    unsafe
                    {
                        fixed (byte* ptr = DimBuf)
                        {
                            uint width = *(uint*)ptr;
                            uint height = *(uint*)(ptr + 4);
                            Console.WriteLine("Dimensions: " + width + ", " + height);
                            buffer = new byte[width * 4 * height];
                            // Create a new bitmap if the dimensions from before aren't valid anymore or it doesn't exist.
                            if (bmp == null || bmp.Width != width || bmp.Height != height)
                            {
                                bmp = new Bitmap((int)width, (int)height);
                                bounds.Width = (int)width;
                                bounds.Height = (int)height;
                            }
                        }
                    }
                    mode = true;
                    pos = 0;
                }
            }
        }
    }
}