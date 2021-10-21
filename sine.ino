/*
 * The GNU GENERAL PUBLIC LICENSE (GNU GPLv3)
 *
 * Copyright (c) 2021 Marcel Licence
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Dieses Programm ist Freie Software: Sie können es unter den Bedingungen
 * der GNU General Public License, wie von der Free Software Foundation,
 * Version 3 der Lizenz oder (nach Ihrer Wahl) jeder neueren
 * veröffentlichten Version, weiter verteilen und/oder modifizieren.
 *
 * Dieses Programm wird in der Hoffnung bereitgestellt, dass es nützlich sein wird, jedoch
 * OHNE JEDE GEWÄHR,; sogar ohne die implizite
 * Gewähr der MARKTFÄHIGKEIT oder EIGNUNG FÜR EINEN BESTIMMTEN ZWECK.
 * Siehe die GNU General Public License für weitere Einzelheiten.
 *
 * Sie sollten eine Kopie der GNU General Public License zusammen mit diesem
 * Programm erhalten haben. Wenn nicht, siehe <https://www.gnu.org/licenses/>.
 */

/*
 * this file is used to precalculate a sine
 * sine is very slow and this can be used to speed up the execution
 *
 * Author: Marcel Licence
 */

#define SINE_BIT	8UL
#define SINE_CNT	(1<<SINE_BIT)
#define SINE_MSK	((1<<SINE_BIT)-1)
#define SINE_I(i)	((i) >> (32 - SINE_BIT)) /* & SINE_MSK */


float *sine = NULL;

void Sine_Init(void)
{
    sine = (float *)malloc(sizeof(float) * SINE_CNT);
    if (sine == NULL)
    {
        Serial.printf("not enough heap memory for sine buffer!\n");
    }
    for (int i = 0; i < SINE_CNT; i++)
    {
        sine[i] = (float)sin(i * 2.0 * PI / SINE_CNT);
    }
}

/*
 * returns sin(value * 2.0f * PI) from lookup
 */
float SineNorm(float alpha_div2pi)
{
    uint32_t index = ((uint32_t)(alpha_div2pi * ((float)SINE_CNT))) % SINE_CNT;
    return sine[index];
}
