namespace Salakhova_Sharp
{
    partial class Form1
    {
        /// <summary>
        ///  Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        ///  Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        ///  Required method for Designer support - do not modify
        ///  the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.btnConnect = new System.Windows.Forms.Button();
            this.btnDisconnect = new System.Windows.Forms.Button();
            this.txtOutput = new System.Windows.Forms.TextBox();
            this.comboRecipient = new System.Windows.Forms.ComboBox();
            this.textBoxMessage = new System.Windows.Forms.TextBox();
            this.btnSend = new System.Windows.Forms.Button();
            this.SuspendLayout();
            // 
            // btnConnect
            // 
            this.btnConnect.Location = new System.Drawing.Point(30, 30);
            this.btnConnect.Name = "btnConnect";
            this.btnConnect.Size = new System.Drawing.Size(200, 50);
            this.btnConnect.TabIndex = 0;
            this.btnConnect.Text = "Подключиться";
            this.btnConnect.UseVisualStyleBackColor = true;
            this.btnConnect.Click += new System.EventHandler(this.btnConnect_Click);
            // 
            // btnDisconnect
            // 
            this.btnDisconnect.Location = new System.Drawing.Point(250, 30);
            this.btnDisconnect.Name = "btnDisconnect";
            this.btnDisconnect.Size = new System.Drawing.Size(200, 50);
            this.btnDisconnect.TabIndex = 1;
            this.btnDisconnect.Text = "Отключиться";
            this.btnDisconnect.UseVisualStyleBackColor = true;
            this.btnDisconnect.Click += new System.EventHandler(this.btnDisconnect_Click);
            // 
            // txtOutput
            // 
            this.txtOutput.Location = new System.Drawing.Point(30, 100);
            this.txtOutput.Multiline = true;
            this.txtOutput.Name = "txtOutput";
            this.txtOutput.ReadOnly = true;
            this.txtOutput.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
            this.txtOutput.Size = new System.Drawing.Size(600, 380);
            this.txtOutput.TabIndex = 2;
            // 
            // comboRecipient
            // 
            this.comboRecipient.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.comboRecipient.FormattingEnabled = true;
            this.comboRecipient.Location = new System.Drawing.Point(30, 500);
            this.comboRecipient.Name = "comboRecipient";
            this.comboRecipient.Size = new System.Drawing.Size(250, 40);
            this.comboRecipient.TabIndex = 3;
            // 
            // textBoxMessage
            // 
            this.textBoxMessage.Location = new System.Drawing.Point(300, 500);
            this.textBoxMessage.Name = "textBoxMessage";
            this.textBoxMessage.Size = new System.Drawing.Size(330, 39);
            this.textBoxMessage.TabIndex = 4;
            // 
            // btnSend
            // 
            this.btnSend.Location = new System.Drawing.Point(30, 560);
            this.btnSend.Name = "btnSend";
            this.btnSend.Size = new System.Drawing.Size(600, 50);
            this.btnSend.TabIndex = 5;
            this.btnSend.Text = "Отправить сообщение";
            this.btnSend.UseVisualStyleBackColor = true;
            this.btnSend.Click += new System.EventHandler(this.btnSend_Click);
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(13F, 32F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(664, 650);
            this.Controls.Add(this.btnSend);
            this.Controls.Add(this.textBoxMessage);
            this.Controls.Add(this.comboRecipient);
            this.Controls.Add(this.txtOutput);
            this.Controls.Add(this.btnDisconnect);
            this.Controls.Add(this.btnConnect);
            this.Margin = new System.Windows.Forms.Padding(5);
            this.Name = "Form1";
            this.Text = "Message Client";
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Button btnConnect;
        private System.Windows.Forms.Button btnDisconnect;
        private System.Windows.Forms.TextBox txtOutput;
        private System.Windows.Forms.ComboBox comboRecipient;
        private System.Windows.Forms.TextBox textBoxMessage;
        private System.Windows.Forms.Button btnSend;
    }
}