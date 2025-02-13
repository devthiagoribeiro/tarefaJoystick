#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "inc/ssd1306.h"
#include "hardware/i2c.h"
#include <string.h>
#include <time.h>

// Definição de macros e variáveis globais
#define LED_AZUL 12
#define LED_VERMELHO 13
#define LED_VERDE 11
#define BTN_JOYSTICK 22
#define BTN_A 5
#define VRY 26
#define VRX 26
#define WRAP 2000
const uint I2C_SDA = 14;
const uint I2C_SCL = 15;
struct render_area *frame_area_ptr; //Variável global para conseguir manipular o display oled em qualquer função
bool ledVerdeStatus = false; //Variável para controle de estado do led verde
static volatile int indice = 0;
static volatile int lastTimeJoy = 0;//Variável para debouncing do botão do joystick
static volatile int lastTimeA = 0; //Variável para debouncing do botão A
bool pwmControl = true; // Variável para habilitar/desabilitar os leds controlados via pwm pelo joystick
void (*arraydefuncoes[])(uint8_t *) = {border0, border1, border2}; // Criando um vetor de ponteiros para funções de bordas do display

//Função de inicialização dos periféricos usados no projeto
struct render_area init(){
    //inicialização básica dos GPIOS
    gpio_init(LED_VERDE);
    gpio_set_dir(LED_VERDE, GPIO_OUT);
    gpio_init(BTN_JOYSTICK);
    gpio_set_dir(BTN_JOYSTICK, GPIO_IN);
    gpio_pull_up(BTN_JOYSTICK);
    gpio_init(BTN_A);
    gpio_set_dir(BTN_A, GPIO_IN);
    gpio_pull_up(BTN_A);
    // Inicializa o conversor analógico-digital (ADC)
    adc_init();
    // Configura os pinos GPIO 26 e 27 como entradas de ADC (alta impedância, sem resistores pull-up)
    adc_gpio_init(VRY);
    adc_gpio_init(VRX);
    // Inicializa o PWM do led azul
    gpio_set_function(LED_AZUL, GPIO_FUNC_PWM); //Seta o pino 12 para funcionar como PWM
    uint slice = pwm_gpio_to_slice_num(LED_AZUL); // Salva o slice referente ao pino 12 na variável "slice"
    pwm_set_clkdiv(slice, 2.0); // Seta o divisor inteiro do clock
    pwm_set_wrap(slice, WRAP); //definir o valor de wrap – valor máximo do contador PWM
    pwm_set_gpio_level(LED_AZUL, 0); //definir o ciclo de trabalho (duty cycle) do pwm
    pwm_set_enabled(slice, true); //habilitar o pwm no slice correspondente
    // Inicializa o PWM do led vermelho
    gpio_set_function(LED_VERMELHO, GPIO_FUNC_PWM); //Seta o pino 13 para funcionar como PWM
    uint slice2 = pwm_gpio_to_slice_num(LED_VERMELHO); // Salva o slice referente ao pino 13 na variável "slice"
    pwm_set_clkdiv(slice2, 2.0); // Seta o divisor inteiro do clock
    pwm_set_wrap(slice2, WRAP); //definir o valor de wrap – valor máximo do contador PWM
    pwm_set_gpio_level(LED_VERMELHO, 0); //definir o ciclo de trabalho (duty cycle) do pwm
    pwm_set_enabled(slice2, true); //habilitar o pwm no slice correspondente

    // Inicialização do i2c
    i2c_init(i2c1, ssd1306_i2c_clock * 1000); 
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Processo de inicialização completo do OLED SSD1306
    ssd1306_init();

    // Preparar área de renderização para o display (ssd1306_width pixels por ssd1306_n_pages páginas)
    struct render_area frame_area = {
        start_column : 0,
        end_column : ssd1306_width - 1,
        start_page : 0,
        end_page : ssd1306_n_pages - 1
    };

    calculate_render_area_buffer_length(&frame_area);
    return frame_area;
}

//Função responsável por imprimir no display oled todas as informações necessárias
void draw_joystick_square(uint x_raw, uint y_raw) {

    //Cálculo de posição do quadrado em relação ao display
    uint x_pos = (x_raw * (128 - 8)) / 39;
    uint y_pos = 56 - ((y_raw * (64 - 8)) / 39);
    // Garantir que o valor fique dentro do intervalo válido
    if(x_pos >= 120) x_pos = 118;
    if(y_pos > 56) y_pos = 56;

    // zera o display inteiro
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    ssd1306_send_buffer(ssd, ssd1306_buffer_length);

    restart:
    
    //Seleciona qual estilo de borda a ser aplicado junto ao quadrado
    switch (indice)
    {
    case 0:
        ssd1306_draw_square(ssd, x_pos, y_pos,8); //Desenha um quadrado de 8x8 pixels
        render_on_display(ssd, frame_area_ptr); //Renderiza tudo no display
        break;
    
    case 1:
        arraydefuncoes[indice - 1](ssd);
        ssd1306_draw_square(ssd, x_pos, y_pos,8); //Desenha um quadrado de 8x8 pixels
        render_on_display(ssd, frame_area_ptr); //Renderiza tudo no display
        break;
    case 2:
        arraydefuncoes[indice - 1](ssd);
        ssd1306_draw_square(ssd, x_pos, y_pos,8); //Desenha um quadrado de 8x8 pixels
        render_on_display(ssd, frame_area_ptr); //Renderiza tudo no display
        break;
    case 3:
        arraydefuncoes[indice - 1](ssd);
        ssd1306_draw_square(ssd, x_pos, y_pos,8); //Desenha um quadrado de 8x8 pixels
        render_on_display(ssd, frame_area_ptr); //Renderiza tudo no display
        break;
    default:
        ssd1306_draw_square(ssd, x_pos, y_pos,8); //Desenha um quadrado de 8x8 pixels
        render_on_display(ssd, frame_area_ptr); //Renderiza tudo no display
        break;
    }
}

// Função para mapear a intensidade dos leds de acordo com a posição do joystick
int calcularIntensidade(int i) {
    int intensidade;
    float m0 = -255.94; // Inclinação calculada
    float b0 = 4095.0; // Coeficiente linear calculado

    float m1 = 240.88; // Inclinação calculada
    float b1 = -5299.36; // Coeficiente linear calculado

    if(i <= 14){
        intensidade = (int)(m0 * i + b0); // Cálculo da intensidade para valores do eixo abaixo da metade
    }else{
        intensidade = (int)(m1 * i + b1); // Cálculo da intensidade para valores do eixo acima da metade
    }
    

    // Garantir que o valor fique dentro do intervalo válido
    if (intensidade < 0) intensidade = 0;
    if (intensidade > 4095) intensidade = 4095;

    return intensidade;
}

//Faz o tratamento das posições dos eixos X e Y
void ledsControl(uint bar_x_pos, uint bar_y_pos){

    //Verifica se o eixo Y está em repouso, acima da metade ou abaixo da metade para controlar os leds
    if(bar_y_pos < 22 && bar_y_pos > 16){
        pwm_set_gpio_level(LED_AZUL, 0);
    }else if(bar_y_pos < 16){
        pwmControl?pwm_set_gpio_level(LED_AZUL, calcularIntensidade(bar_y_pos)):pwm_set_gpio_level(LED_AZUL, 0);
    }else if(bar_y_pos > 24){
        pwmControl?pwm_set_gpio_level(LED_AZUL, calcularIntensidade(bar_y_pos)):pwm_set_gpio_level(LED_AZUL, 0);
    }

    //Verifica se o eixo X está em repouso, acima da metade ou abaixo da metade para controlar os leds
    if(bar_x_pos < 22 && bar_x_pos > 16){
        pwm_set_gpio_level(LED_VERMELHO, 0);
    }else if(bar_x_pos < 16){
        pwmControl?pwm_set_gpio_level(LED_VERMELHO, calcularIntensidade(bar_x_pos)):pwm_set_gpio_level(LED_VERMELHO, 0);
    }else if(bar_x_pos > 24){
        pwmControl?pwm_set_gpio_level(LED_VERMELHO, calcularIntensidade(bar_x_pos)):pwm_set_gpio_level(LED_VERMELHO, 0);
    }

    //Desenha o quadrado na posição especifica no display
    draw_joystick_square(bar_x_pos, bar_y_pos);
}

//rotina de interrupção dos botões
void gpio_irq_handler(uint gpio, uint32_t events){
    uint32_t currentTime = to_us_since_boot(get_absolute_time());
    //tratamento debouncing
    if(gpio == BTN_JOYSTICK && currentTime - lastTimeJoy > 200000){
        lastTimeJoy = currentTime;
        ledVerdeStatus = !ledVerdeStatus;
        gpio_put(LED_VERDE, ledVerdeStatus);
        indice == 3 ? indice = 0: indice++; //Atualiza o indíce do estilo de borda a ser mostrado
    }else if(gpio == BTN_A && currentTime - lastTimeA > 200000){
        lastTimeA = currentTime;
        pwmControl = !pwmControl;
    }
}

int main() {
    // Inicializa as interfaces de entrada e saída padrão (UART)
    stdio_init_all();
    //Inicializa os periféricos e tem como retorno o frame area
    struct render_area frame_area = init();
    frame_area_ptr = &frame_area;

    //Ativa as interrupções com callback para os botões A e Joystick
    gpio_set_irq_enabled_with_callback(BTN_JOYSTICK, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BTN_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    //Armazenam as posições anteriores dos potenciometros para detectar se houve mudança
    uint xAnt, yAnt;

    // Inicia o loop infinito para leitura e exibição dos valores do joystick
    while (1) {
        // Seleciona a entrada ADC 0 (conectada ao eixo X do joystick)
        adc_select_input(1);
        // Lê o valor do ADC para o eixo X
        uint adc_x_raw = adc_read();
        
        // Seleciona a entrada ADC 1 (conectada ao eixo Y do joystick)
        adc_select_input(0);
        // Lê o valor do ADC para o eixo Y
        uint adc_y_raw = adc_read();

        // Configuração para estabilizar os valores dos eixos entre 0 e 39
        const uint bar_width = 40;
        const uint adc_max = (1 << 12) - 1; 
        uint bar_x_pos = adc_x_raw * bar_width / adc_max;
        uint bar_y_pos = adc_y_raw * bar_width / adc_max;
       
        //Faz o tratamento das posições X e Y e de acordo com isso controla leds via pwm e exibe o quadrado no display oled
        if(bar_x_pos != xAnt || bar_y_pos != yAnt) ledsControl(bar_x_pos, bar_y_pos);
        xAnt = bar_x_pos;
        yAnt = bar_y_pos;
        // Pausa o programa por 10 milissegundos antes de ler novamente
        sleep_ms(10);
    }
}