/**
 * @brief ProtoZOA_Main Primary Loop
 * 
 * The ProtoZOA USB MIDI 2.0 Prototyping Tool, an open source project in conjunction of
 * members of the MIDI Association and AMEI to prototype and develop MIDI 2.0
 * concepts and features. The firmware is provided to members of the MIDI Association
 * and AMEI based on the indicated license. It is expected that members of the
 * licensed group will contibute to the firmware so we can cooperatively improve and
 * make consistent the implementations of MIDI 2.0.
 * 
 * COPYRIGHT (c) 2022 AMENOTE INC.
 *
 * This Software is subject to the AmeNote ProtoZOA Firmware License terms and conditions
 * outlined in the project README and may or may not become public, open-source at
 * some point in the future as decided by AmeNote, the MIDI Association and AMEI.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NON- INFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 * 
 * Created by: Michael Loh and Andrew Mee for AmeNote Inc.
 * Date: June 17, 2022
*/

#ifndef PROTOZOA_MAIN_H
#define PROTOZOA_MAIN_H
void ProtoZOA_Main_setup();
void ProtoZOA_MAIN_task();
#endif // PROTOZOA_MAIN_H
