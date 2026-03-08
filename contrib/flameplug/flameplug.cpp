/*
   FlamePlug - Fractal Flame Editor for NetRadiant
   Based on the Fractal Flame Algorithm (Scott Draves, Erik Reckase, 2003)
   Inspired by Apophysis 7x (https://github.com/wanily/apophysis7x)

   Creates flares, textures, and procedural content using IFS (Iterated Function Systems).
*/

#include "debugging/debugging.h"
#include "iplugin.h"
#include "string/string.h"
#include "stream/stringstream.h"
#include "modulesystem/singletonmodule.h"
#include "typesystem.h"
#include "qerplugin.h"

#include <QApplication>
#include <QDialog>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QFileDialog>
#include <QImage>
#include <QPainter>
#include <cmath>
#include <random>
#include <vector>
#include <algorithm>

namespace FlamePlug
{
QWidget* main_window;
char MenuList[128] = "";

// --- Minimal Fractal Flame Renderer (chaos game + variations) ---
// Paper: Draves & Reckase, "The Fractal Flame Algorithm", 2003

struct Vec2 { double x, y; };

// Variation 0: linear (identity)
inline Vec2 V0( Vec2 p ){ return p; }

// Variation 1: sinusoidal
inline Vec2 V1( Vec2 p ){ return { sin( p.x ), sin( p.y ) }; }

// Variation 2: spherical
inline Vec2 V2( Vec2 p ){
	double r2 = p.x * p.x + p.y * p.y;
	if ( r2 < 1e-12 ) return p;
	return { p.x / r2, p.y / r2 };
}

// Variation 3: swirl
inline Vec2 V3( Vec2 p ){
	double r2 = p.x * p.x + p.y * p.y;
	double s = sin( r2 ), c = cos( r2 );
	return { p.x * s - p.y * c, p.x * c + p.y * s };
}

// Variation 4: horseshoe
inline Vec2 V4( Vec2 p ){
	double r = sqrt( p.x * p.x + p.y * p.y );
	if ( r < 1e-12 ) return p;
	return { ( p.x - p.y ) * ( p.x + p.y ) / r, 2 * p.x * p.y / r };
}

// Variation 5: polar
inline Vec2 V5( Vec2 p ){
	double r = sqrt( p.x * p.x + p.y * p.y );
	double th = atan2( p.y, p.x );
	return { th / 3.14159, r - 1 };
}

// Variation 6: disc
inline Vec2 V6( Vec2 p ){
	double r = sqrt( p.x * p.x + p.y * p.y );
	double th = atan2( p.y, p.x );
	return { th / 3.14159 * sin( 3.14159 * r ), th / 3.14159 * cos( 3.14159 * r ) };
}

// Variation 7: heart
inline Vec2 V7( Vec2 p ){
	double r = sqrt( p.x * p.x + p.y * p.y );
	double th = atan2( p.y, p.x );
	return { r * sin( th * r ), -r * cos( th * r ) };
}

// Variation 8: fisheye
inline Vec2 V8( Vec2 p ){
	double r = sqrt( p.x * p.x + p.y * p.y );
	if ( r < 1e-12 ) return p;
	return { 2 * p.y / ( r + 1 ), 2 * p.x / ( r + 1 ) };
}

// Variation 9: bubble
inline Vec2 V9( Vec2 p ){
	double r2 = p.x * p.x + p.y * p.y;
	return { 4 * p.x / ( r2 + 4 ), 4 * p.y / ( r2 + 4 ) };
}

// Variation 10: exponential
inline Vec2 V10( Vec2 p ){
	double ex = exp( p.x - 1 );
	return { ex * cos( 3.14159 * p.y ), ex * sin( 3.14159 * p.y ) };
}

static Vec2 applyVariation( int v, Vec2 p ){
	switch ( v ) {
		case 0: return V0( p );
		case 1: return V1( p );
		case 2: return V2( p );
		case 3: return V3( p );
		case 4: return V4( p );
		case 5: return V5( p );
		case 6: return V6( p );
		case 7: return V7( p );
		case 8: return V8( p );
		case 9: return V9( p );
		case 10: return V10( p );
		default: return V0( p );
	}
}

// Affine: (ax+by+c, dx+ey+f)
struct Affine {
	double a, b, c, d, e, f;
	Vec2 apply( Vec2 p ) const {
		return { a * p.x + b * p.y + c, d * p.x + e * p.y + f };
	}
};

struct FlameFunc {
	Affine affine;
	int variation;  // 0-4
	double weight;
	double color;
};

enum Preset { Preset_Flame, Preset_Flare, Preset_Spiral, Preset_Organic, Preset_Star };
constexpr int MAX_FUNCS = 5;

void getPresetFuncs( Preset preset, FlameFunc* out, int& n ){
	switch ( preset ) {
		case Preset_Flame:
			out[0] = { { 0.5, 0, 0, 0, 0.5, 0 }, 0, 1.0, 0.0 };
			out[1] = { { 0.5, 0, 0.5, 0, 0.5, 0 }, 1, 1.0, 0.33 };
			out[2] = { { 0.5, 0, 0.25, 0, 0.5, 0.5 }, 3, 1.0, 0.66 };
			n = 3;
			break;
		case Preset_Flare:
			out[0] = { { 0.5, 0, 0, 0, 0.5, 0 }, 2, 1.0, 0.0 };   // spherical
			out[1] = { { 0.4, 0, 0.5, 0, 0.4, 0.5 }, 3, 1.0, 0.5 }; // swirl
			out[2] = { { 0.3, 0.1, 0.3, -0.1, 0.3, 0.4 }, 8, 0.8, 0.8 }; // fisheye
			n = 3;
			break;
		case Preset_Spiral:
			out[0] = { { 0.5, 0, 0, 0, 0.5, 0 }, 0, 1.0, 0.0 };
			out[1] = { { 0.5, 0.2, 0.3, -0.2, 0.5, 0.5 }, 3, 1.0, 0.4 };
			out[2] = { { 0.4, 0, 0.5, 0, 0.4, 0.3 }, 6, 0.5, 0.7 }; // disc
			n = 3;
			break;
		case Preset_Organic:
			out[0] = { { 0.5, 0, 0, 0, 0.5, 0 }, 1, 1.0, 0.0 };
			out[1] = { { 0.5, 0.1, 0.4, -0.1, 0.5, 0.4 }, 8, 1.0, 0.35 };
			out[2] = { { 0.5, 0, 0.25, 0, 0.5, 0.5 }, 3, 1.0, 0.66 };
			out[3] = { { 0.4, 0, 0.5, 0, 0.4, 0.5 }, 9, 0.5, 0.5 }; // bubble
			n = 4;
			break;
		case Preset_Star:
			out[0] = { { 0.5, 0, 0, 0, 0.5, 0 }, 4, 1.0, 0.0 };   // horseshoe
			out[1] = { { 0.5, 0, 0.5, 0, 0.5, 0 }, 2, 1.0, 0.5 };  // spherical
			out[2] = { { 0.4, 0.1, 0.3, -0.1, 0.4, 0.5 }, 7, 0.8, 0.8 }; // heart
			n = 3;
			break;
		default:
			getPresetFuncs( Preset_Flame, out, n );
	}
}

void renderFlame( std::vector<uint32_t>& rgba, int w, int h, int samples, double gamma, Preset preset ){
	FlameFunc funcs[MAX_FUNCS];
	int nfuncs;
	getPresetFuncs( preset, funcs, nfuncs );

	double weightSum = 0;
	for ( int i = 0; i < nfuncs; ++i ) weightSum += funcs[i].weight;
	if ( weightSum < 1e-6 ) weightSum = 1;
	for ( int i = 0; i < nfuncs; ++i ) funcs[i].weight /= weightSum;

	// Histogram: 4 channels (r,g,b,alpha) per pixel, log-density display
	int size = w * h;
	std::vector<double> histR( size, 0 ), histG( size, 0 ), histB( size, 0 ), histA( size, 0 );

	std::mt19937 rng( 12345 );
	std::uniform_real_distribution<double> unif( -1, 1 );
	std::uniform_real_distribution<double> unif01( 0, 1 );

	Vec2 p = { unif( rng ), unif( rng ) };
	double c = unif01( rng );

	const int burnin = 20;
	int total = burnin + samples;

	for ( int i = 0; i < total; ++i ) {
		// Weighted random function choice
		double r = unif01( rng );
		int idx = 0;
		double sum = 0;
		for ( int j = 0; j < nfuncs; ++j ) {
			sum += funcs[j].weight;
			if ( r < sum ) { idx = j; break; }
		}

		FlameFunc& f = funcs[idx];
		Vec2 q = f.affine.apply( p );

		q = applyVariation( f.variation, q );

		p = q;
		c = ( c + f.color ) * 0.5;

		if ( i >= burnin ) {
			// Map [-1,1] to pixel coords (centered)
			int px = (int)( ( p.x + 1 ) * 0.5 * ( w - 1 ) );
			int py = (int)( ( p.y + 1 ) * 0.5 * ( h - 1 ) );
			if ( px >= 0 && px < w && py >= 0 && py < h ) {
				int idx2 = py * w + px;
				// Structural coloring: hue from c
				double hue = c * 6;
				int hi = (int)hue % 6;
				double f = hue - floor( hue );
				double q = 1 - f, t = f;
				double r, g, b;
				if ( hi == 0 ) { r = 1; g = t; b = 0; }
				else if ( hi == 1 ) { r = q; g = 1; b = 0; }
				else if ( hi == 2 ) { r = 0; g = 1; b = t; }
				else if ( hi == 3 ) { r = 0; g = q; b = 1; }
				else if ( hi == 4 ) { r = t; g = 0; b = 1; }
				else { r = 1; g = 0; b = q; }
				histR[idx2] += r;
				histG[idx2] += g;
				histB[idx2] += b;
				histA[idx2] += 1;
			}
		}
	}

	// Log-density display + gamma
	double maxA = 0;
	for ( int i = 0; i < size; ++i )
		if ( histA[i] > maxA ) maxA = histA[i];
	if ( maxA < 1e-6 ) maxA = 1;

	for ( int i = 0; i < size; ++i ) {
		double a = histA[i];
		if ( a < 1e-6 ) {
			rgba[i] = 0xFF000000;
			continue;
		}
		double scale = log( 1 + a ) / log( 1 + maxA );
		scale = pow( scale, 1.0 / gamma );
		double r = ( histR[i] / a ) * scale;
		double g = ( histG[i] / a ) * scale;
		double b = ( histB[i] / a ) * scale;
		r = std::min( 1.0, std::max( 0.0, r ) );
		g = std::min( 1.0, std::max( 0.0, g ) );
		b = std::min( 1.0, std::max( 0.0, b ) );
		rgba[i] = 0xFF000000 | ( (int)( r * 255 ) << 16 ) | ( (int)( g * 255 ) << 8 ) | (int)( b * 255 );
	}
}

// --- Plugin ---
const char* init( void* hApp, void* pMainWidget ){
	main_window = static_cast<QWidget*>( pMainWidget );
	return "Fractal Flame Editor for flares, textures, content";
}

const char* getName(){
	return "FlamePlug";
}

const char* getCommandList(){
	strcpy( MenuList, "Fractal Flame Editor..." );
	return MenuList;
}

const char* getCommandTitleList(){
	return "";
}

void openFlameEditor(){
	QDialog dialog( main_window, Qt::Dialog | Qt::WindowCloseButtonHint );
	dialog.setWindowTitle( "Fractal Flame Editor" );
	dialog.setMinimumSize( 400, 420 );

	auto* form = new QFormLayout( &dialog );
	form->setSizeConstraint( QLayout::SizeConstraint::SetNoConstraint );

	auto* preview = new QLabel();
	preview->setMinimumSize( 256, 256 );
	preview->setAlignment( Qt::AlignCenter );
	preview->setStyleSheet( "background: #1a1a1a; border: 1px solid #444;" );
	preview->setText( "Click Render to generate" );
	form->addRow( preview );

	auto* presetCombo = new QComboBox();
	presetCombo->addItem( "Flame (sinusoidal + swirl)" );
	presetCombo->addItem( "Flare (spherical + fisheye)" );
	presetCombo->addItem( "Spiral (swirl + disc)" );
	presetCombo->addItem( "Organic (multi-variation)" );
	presetCombo->addItem( "Star (horseshoe + heart)" );
	form->addRow( "Preset:", presetCombo );

	auto* samplesSpin = new QSpinBox();
	samplesSpin->setRange( 10000, 5000000 );
	samplesSpin->setValue( 200000 );
	samplesSpin->setSingleStep( 50000 );
	form->addRow( "Samples:", samplesSpin );

	auto* gammaSpin = new QDoubleSpinBox();
	gammaSpin->setRange( 0.5, 5.0 );
	gammaSpin->setValue( 2.2 );
	gammaSpin->setSingleStep( 0.1 );
	form->addRow( "Gamma:", gammaSpin );

	auto* renderBtn = new QPushButton( "Render" );
	form->addRow( renderBtn );

	auto* exportResCombo = new QComboBox();
	exportResCombo->addItem( "256×256", 256 );
	exportResCombo->addItem( "512×512", 512 );
	exportResCombo->addItem( "1024×1024", 1024 );
	form->addRow( "Export size:", exportResCombo );

	auto* exportBtn = new QPushButton( "Export as PNG..." );
	exportBtn->setEnabled( false );
	form->addRow( exportBtn );

	QImage lastImage;

	QObject::connect( renderBtn, &QPushButton::clicked, [&](){
		renderBtn->setEnabled( false );
		renderBtn->setText( "Rendering..." );
		QApplication::processEvents();

		int w = 256, h = 256;
		std::vector<uint32_t> rgba( w * h );
		Preset p = (Preset)presetCombo->currentIndex();
		renderFlame( rgba, w, h, samplesSpin->value(), gammaSpin->value(), p );

		lastImage = QImage( w, h, QImage::Format::Format_ARGB32 );
		for ( int y = 0; y < h; ++y )
			for ( int x = 0; x < w; ++x )
				lastImage.setPixel( x, y, rgba[y * w + x] );

		preview->setPixmap( QPixmap::fromImage( lastImage ).scaled( 256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
		exportBtn->setEnabled( true );
		renderBtn->setEnabled( true );
		renderBtn->setText( "Render" );
	} );

	QObject::connect( exportBtn, &QPushButton::clicked, [&](){
		QString path = QFileDialog::getSaveFileName( &dialog, "Export Flame Texture",
			QString(), "PNG (*.png);;TGA (*.tga)" );
		if ( !path.isEmpty() && !lastImage.isNull() ) {
			QImage toSave = lastImage;
			int exportSize = exportResCombo->currentData().toInt();
			if ( exportSize != 256 ) {
				std::vector<uint32_t> rgba( exportSize * exportSize );
				Preset p = (Preset)presetCombo->currentIndex();
				renderFlame( rgba, exportSize, exportSize, samplesSpin->value() * ( exportSize * exportSize ) / 65536, gammaSpin->value(), p );
				toSave = QImage( exportSize, exportSize, QImage::Format::Format_ARGB32 );
				for ( int y = 0; y < exportSize; ++y )
					for ( int x = 0; x < exportSize; ++x )
						toSave.setPixel( x, y, rgba[y * exportSize + x] );
			}
			if ( toSave.save( path ) )
				GlobalRadiant().m_pfnMessageBox( main_window, "Texture saved.", "FlamePlug", EMessageBoxType::Info, 0 );
			else
				GlobalRadiant().m_pfnMessageBox( main_window, "Failed to save.", "FlamePlug", EMessageBoxType::Error, 0 );
		}
	} );

	auto* buttons = new QDialogButtonBox( QDialogButtonBox::StandardButton::Close );
	form->addWidget( buttons );
	QObject::connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );

	dialog.exec();
}

void dispatch( const char* command, float* vMin, float* vMax, bool bSingleBrush ){
	if ( string_equal( command, "Fractal Flame Editor..." ) )
		openFlameEditor();
}

} // namespace FlamePlug

class FlamePlugPluginDependencies :
	public GlobalRadiantModuleRef
{
};

class FlamePlugModule : public TypeSystemRef
{
	_QERPluginTable m_plugin;
public:
	typedef _QERPluginTable Type;
	STRING_CONSTANT( Name, "FlamePlug" );

	FlamePlugModule(){
		m_plugin.m_pfnQERPlug_Init = &FlamePlug::init;
		m_plugin.m_pfnQERPlug_GetName = &FlamePlug::getName;
		m_plugin.m_pfnQERPlug_GetCommandList = &FlamePlug::getCommandList;
		m_plugin.m_pfnQERPlug_GetCommandTitleList = &FlamePlug::getCommandTitleList;
		m_plugin.m_pfnQERPlug_Dispatch = &FlamePlug::dispatch;
	}
	_QERPluginTable* getTable(){
		return &m_plugin;
	}
};

typedef SingletonModule<FlamePlugModule, FlamePlugPluginDependencies> SingletonFlamePlugModule;
SingletonFlamePlugModule g_FlamePlugModule;

extern "C" void RADIANT_DLLEXPORT Radiant_RegisterModules( ModuleServer& server ){
	initialiseModule( server );
	g_FlamePlugModule.selfRegister();
}
