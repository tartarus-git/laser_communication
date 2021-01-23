﻿using System;
using System.Windows.Forms;
using System.IO.Ports;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

namespace SerialReciever
{
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();
        }

        static int pos = 0;

        // TODO: Make the COM thing variable to avoid presentation mess-ups.
        static SerialPort Serial = new SerialPort("COM3", 9600);
        // TODO: Check that this whole static thing is the best way to go. Is there a better way.
        static PictureBox Picture;

        static Bitmap bmp;
        static Rectangle bounds;

        private void Form1_Load(object sender, EventArgs e)
        {
            Picture = pictureBox;
            Serial.DataReceived += DimReceived;
        }

        static byte[] DimBuf = new byte[8];
        static byte[] buffer;

        static void DimReceived(object sender, SerialDataReceivedEventArgs e)
        {
            int BytesRead = Serial.Read(DimBuf, pos, 8 - pos);
            pos += BytesRead;
            if (pos == 8)
            {
                unsafe
                {
                    fixed (byte* ptr = DimBuf)
                    {
                        int width = *(int*)ptr;
                        int height = *(int*)(ptr + 4);
                        buffer = new byte[width * 4 * height];
                        // Create a new bitmap if the dimensions from before aren't valid anymore or it doesn't exist.
                        if (bmp == null || bmp.Width != width || bmp.Height != height)
                        {
                            bmp = new Bitmap(width, height);
                            bounds.Width = width;
                            bounds.Height = height;
                        }
                    }
                }
                pos = 0;
                Serial.DataReceived -= DimReceived;
                Serial.DataReceived += DataReceived;
            }
        }

        static void DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            int BytesRead = Serial.Read(buffer, pos, buffer.Length - pos);
            pos += BytesRead;
            if (pos == buffer.Length)
            {
                BitmapData bmpData = bmp.LockBits(bounds, ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
                Marshal.Copy(buffer, 0, bmpData.Scan0, buffer.Length);
                bmp.UnlockBits(bmpData);
                pos = 0;
            }
        }
    }
}