using System;
using System.Text;
using System.Windows.Forms;

namespace Salakhova_Sharp
{
    public partial class Form1 : Form
    {
        // Создаем экземпляр нашего нового чистого C# клиента
        private SalakhovaNetworkClient netClient = new SalakhovaNetworkClient();

        private System.Windows.Forms.Timer pollTimer;
        private System.Windows.Forms.Timer pingTimer;
        private string clientName = "Client";

        // Константы протокола твоего сервера
        const int MT_DATA = 1;
        const int MT_QUIT = 4;
        const int MT_INFO = 5;

        public Form1()
        {
            InitializeComponent();
            this.FormClosing += Form1_FormClosing;

            btnConnect.Text = "Connect";
            btnDisconnect.Text = "Disconnect";
            btnSend.Text = "Send Message";
            this.Text = "Message Client (Sockets Pure)";

            // Таймер пуллинга: каждые 50 мс забирает данные из очереди netClient
            pollTimer = new System.Windows.Forms.Timer();
            pollTimer.Interval = 50;
            pollTimer.Tick += PollTimer_Tick;

            // Таймер пинга (отправка регулярного MT_INFO для обновления активности)
            pingTimer = new System.Windows.Forms.Timer();
            pingTimer.Interval = 10000;
            pingTimer.Tick += PingTimer_Tick;

            ToggleUi(false);
        }

        private void ToggleUi(bool isConnected)
        {
            btnConnect.Enabled = !isConnected;
            btnDisconnect.Enabled = isConnected;
            btnSend.Enabled = isConnected;
            comboRecipient.Enabled = isConnected;
            textBoxMessage.Enabled = isConnected;
        }

        private void btnConnect_Click(object sender, EventArgs e)
        {
            // Здесь можно вызвать диалог ввода имени, как в Velgan-примере
            if (netClient.Connect("127.0.0.1", 12345, clientName))
            {
                pollTimer.Start();
                pingTimer.Start();
                ToggleUi(true);
            }
            else
            {
                MessageBox.Show("Server is not running or connection failed!", "Connection Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void btnDisconnect_Click(object sender, EventArgs e)
        {
            DisconnectClient();
        }

        private void btnSend_Click(object sender, EventArgs e)
        {
            if (!netClient.IsConnected || string.IsNullOrWhiteSpace(textBoxMessage.Text)) return;
            if (comboRecipient.SelectedItem == null) return;

            int targetId = ((RecipientItem)comboRecipient.SelectedItem).Id;

            // Отправляем текстовые данные (MT_DATA = 1 в твоем перечислении enum)
            netClient.Send(targetId, (MessageTypes)MT_DATA, textBoxMessage.Text);

            txtOutput.AppendText($"[You -> {comboRecipient.SelectedItem}]: {textBoxMessage.Text}\r\n");
            textBoxMessage.Clear();
        }

        private void PingTimer_Tick(object sender, EventArgs e)
        {
            if (netClient.IsConnected)
            {
                // Отправляем пинг-сообщение (MT_INFO = 5) на сервер (target = -2)
                netClient.Send(-2, (MessageTypes)MT_INFO, "");
            }
        }

        // РЕАЛИЗАЦИЯ ПУЛЛИНГА:
        // Метод не делает блокирующих запросов в сеть, он мгновенно выгребает сообщения
        // из безопасной локальной очереди, которую наполняет фоновый поток ReaderLoop.
        private void PollTimer_Tick(object sender, EventArgs e)
        {
            int srcId;
            MessageTypes type;
            string text;

            while (netClient.Poll(out srcId, out type, out text))
            {
                // Если тип сообщения равен 6 (MT_CONFIRM) — пришел список клиентов
                if ((int)type == 6)
                {
                    ParseClientList(text);
                }
                // Если тип сообщения равен 1 (MT_DATA) — это обычный текст от другого клиента
                else if ((int)type == 1)
                {
                    txtOutput.AppendText($"[From Client #{srcId}]: {text}\r\n");
                }
            }

            // Проверяем статус сетевого подключения
            if (!netClient.IsConnected)
            {
                pollTimer.Stop();
                pingTimer.Stop();
                ToggleUi(false);
                MessageBox.Show("Connection to server lost!", "Disconnected", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }
        }

        private void ParseClientList(string payload)
        {
            int prevId = -999;
            if (comboRecipient.SelectedItem != null)
                prevId = ((RecipientItem)comboRecipient.SelectedItem).Id;

            comboRecipient.Items.Clear();
            comboRecipient.Items.Add(new RecipientItem("All (Broadcast)", -1));

            if (!string.IsNullOrWhiteSpace(payload))
            {
                string[] clients = payload.Split(new[] { ';' }, StringSplitOptions.RemoveEmptyEntries);
                foreach (var c in clients)
                {
                    string[] parts = c.Split(':');
                    if (parts.Length >= 2 && int.TryParse(parts[0], out int id))
                    {
                        // Не добавляем в список получателей самих себя
                        if (id != netClient.ClientId)
                        {
                            comboRecipient.Items.Add(new RecipientItem($"Client #{id}", id));
                        }
                    }
                }
            }

            bool found = false;
            foreach (RecipientItem item in comboRecipient.Items)
            {
                if (item.Id == prevId) { comboRecipient.SelectedItem = item; found = true; break; }
            }
            if (!found && comboRecipient.Items.Count > 0) comboRecipient.SelectedIndex = 0;
        }

        private void DisconnectClient()
        {
            if (netClient.IsConnected)
            {
                pollTimer.Stop();
                pingTimer.Stop();
                netClient.Disconnect();
                ToggleUi(false);
                comboRecipient.Items.Clear();
            }
        }

        private void Form1_FormClosing(object sender, FormClosingEventArgs e)
        {
            DisconnectClient();
        }
    }
    public class RecipientItem
    {
        public string Name { get; }
        public int Id { get; }

        public RecipientItem(string name, int id)
        {
            Name = name;
            Id = id;
        }

        public override string ToString() => Name;
    }
}