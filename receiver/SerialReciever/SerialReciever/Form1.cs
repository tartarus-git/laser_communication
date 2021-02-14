using System;
using System.Windows.Forms;
using System.IO.Ports;
using System.Drawing;
using System.Drawing.Drawing2D;
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

        // Create a derived class from PictureBox so that we can change how the picture box renders the image.
        class CrispPictureBox : PictureBox
        {
            /*protected override void OnPaint(PaintEventArgs pe)
            {
                // This stuff makes it so that the pixels don't get blurred. Instead, they are very crisp, hence the class name.
                pe.Graphics.InterpolationMode = InterpolationMode.NearestNeighbor;
                pe.Graphics.PixelOffsetMode = PixelOffsetMode.Half;
                base.OnPaint(pe);
            }*/
        }

        // TODO: Make the COM thing variable to avoid presentation mess-ups.
        static SerialPort Serial;
        // TODO: Check that this whole static thing is the best way to go. Is there a better way.
        static CrispPictureBox Picture;

        static bool isAlive = true;
        Thread receiveThread;

        Form prompt;

        static byte[] dimBuf = new byte[8];
        static byte[] buffer;
        static int pos = 0;
        static int BytesRead;

        static Bitmap bmp = null;
        static BitmapData bmpData;
        static Rectangle bounds = Rectangle.Empty;

        private void submitClicked(object sender, EventArgs e)
        {
            prompt.Close();
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            // Create a crisp picture box and set the image to the bitmap.
            Picture = new CrispPictureBox();
            Picture.Size = ClientSize;
            Picture.Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right;
            Picture.SizeMode = PictureBoxSizeMode.Zoom;
            this.Controls.Add(Picture);

            // Show a prompt to get the com port from the user.
            prompt = new Form();

            TextBox input = new TextBox();
            prompt.Controls.Add(input);

            Button submit = new Button();
            submit.Location = new Point(submit.Location.X, input.Size.Height);
            submit.Text = "Submit";
            submit.Click += submitClicked;
            prompt.Controls.Add(submit);

            prompt.ShowDialog();

            // Open up a SerialPort for communication with the Arduino.
            Serial = new SerialPort(input.Text, 115200);
            Serial.Open();

            // Send some useless data so the receive LED on the Arduino blinks.
            Serial.Write(Encoding.ASCII.GetBytes("A"), 0, 1);

            Serial.DiscardInBuffer(); // This is necessary because there is still stuff left in there from the program upload to arduino or something. Really weird. Find out more about it if you can. TODO.
            
            // Start a new thread on which to receive data from the serial connection.
            receiveThread = new Thread(new ThreadStart(receive));
            receiveThread.Start();

            this.FormClosing += closing;
        }

        private void closing(object sender, EventArgs e)
        {
            isAlive = false;
            receiveThread.Join();
        }

        static void UpdateImage()
        {
            // Add newly received chunk to the bitmap and display again so that you can see the image being built.
            bmpData = bmp.LockBits(bounds, ImageLockMode.WriteOnly, PixelFormat.Format1bppIndexed);
            Marshal.Copy(buffer, pos, bmpData.Scan0 + pos, BytesRead);
            bmp.UnlockBits(bmpData);
            Picture.Image = bmp;
        }

        // TODO: I know you're confused because of c++, but use the uppercase naming scheme for C#, it looks better.

        static void receive()
        {
            while (isAlive)
            {
                int length = 0;

                // Start receiving image dimensions first.
                while (isAlive)
                {
                    BytesRead = Serial.Read(dimBuf, pos, 8 - pos);              // TODO: Make this async so program doesn't freeze when the image is done.
                    pos += BytesRead;
                    if (pos == 8)
                    {
                        Console.WriteLine("Received image dimensions.");
                        unsafe
                        {
                            fixed (byte* ptr = dimBuf)
                            {
                                int width = *(int*)ptr;
                                int height = *(int*)(ptr + 4);
                                Console.WriteLine("Dimensions: " + width + ", " + height);

                                // Create buffer and bitmap and bounds for LockBits.
                                length = width * height;            // TODO: This obviously isn't right if we're using 1bpp. Fix this.
                                buffer = new byte[length];
                                bmp = new Bitmap(width, height);
                                bounds = new Rectangle(0, 0, width, height);

                                break;
                            }
                        }
                    }
                }

                pos = 0;

                // Start receiving actual image data.
                while (isAlive)
                {
                    BytesRead = Serial.Read(buffer, pos, length - pos);

                    // Invoke the image updater on the PictureBox thread (which is the main thread) because Bitmap isn't thread safe
                    // and the picture box sometimes uses it when we're using it, which isn't good.
                    // This will prevent that because it has to be done synchronously now.
                    Picture.Invoke(new MethodInvoker(UpdateImage));

                    pos += BytesRead;
                    if (pos == length)                  // TODO: This obviously won't work with 1bpp.
                    {
                        Console.WriteLine("Received the entire image. Ready to receive the next one.");
                        break;
                    }
                }
            }
        }
    }
}