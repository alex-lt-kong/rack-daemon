import * as React from 'react';
import {createRoot} from 'react-dom/client';
import Slider from '@mui/material/Slider';
import axios from 'axios';
import CardHeader from '@mui/material/CardHeader';
import Grid from '@mui/material/Grid'; // Grid version 1
import AppBar from '@mui/material/AppBar';
import Box from '@mui/material/Box';
import Toolbar from '@mui/material/Toolbar';
import Typography from '@mui/material/Typography';
import Button from '@mui/material/Button';
import Card from '@mui/material/Card';
import CardActions from '@mui/material/CardActions';
import CardContent from '@mui/material/CardContent';
import CardMedia from '@mui/material/CardMedia';

class DoorState extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      doorStates: {}
    };
  }

  componentDidMount() {
    axios.get('../get_rack_door_states_json/')
        .then((response) => {
          this.setState({
            doorStates: response.data.data
          }, ()=>{
            console.log(this.state.doorStates);
          });
        })
        .catch((error) => {
          console.log(error);
          alert(`${error}`);
        // You canNOT write error.response or whatever similar here.
        // The reason is that this catch() catches both network error and other errors,
        // which may or may not have a response property.
        });
  }

  render() {
    return (
      <></>
    );
  }
}


class LiveImages extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      imagesList: [],
      imageId: 0
    };
    this.onRangeValueChange = this.onRangeValueChange.bind(this);
  }

  componentDidMount() {
    axios.get('../get_images_list_json/')
        .then((response) => {
          this.setState({
            imagesList: response.data.data,
            imageId: response.data.data.length - 1
          }, ()=>{
            console.log(this.state.imagesList);
          });
        })
        .catch((error) => {
          console.log(error);
          alert(`${error}`);
        // You canNOT write error.response or whatever similar here.
        // The reason is that this catch() catches both network error and other errors,
        // which may or may not have a response property.
        });
  }

  onRangeValueChange(event) {
    const parsedValue = parseInt(event.target.value);
    this.setState({
      imageId: parsedValue
    });
  };

  render() {
    if (this.state.imagesList !== null && typeof this.state.imagesList[this.state.imageId] === 'string') {
      return (
        <Card sx={{maxWidth: 480, mt: '2rem'}}>
          <CardMedia
            component="img"
            height="640"
            image={`../get_images_jpg/?imageName=${this.state.imagesList[this.state.imageId]}`}
            alt={this.state.imagesList[this.state.imageId]}
            sx={{objectFit: 'contain'}}
          />
          <CardContent>
            <Typography gutterBottom variant="h5" component="div">
              CCTV
            </Typography>
            <Typography variant="body2" color="text.secondary">
              {this.state.imagesList[this.state.imageId]}
            </Typography>
          </CardContent>
          <CardActions>
            <Slider
              defaultValue={this.state.imagesList.length - 1} aria-label="Default" valueLabelDisplay="auto"
              min={0} max={this.state.imagesList.length - 1} onChange={this.onRangeValueChange}
              sx={{mx: '2rem'}}
            />
          </CardActions>
        </Card>
      );
    }
  }
}

export default function ButtonAppBar() {
  return (
    <Box sx={{flexGrow: 1}}>
      <AppBar position="static">
        <Toolbar>
          <Typography
            variant="h6"
            noWrap
            component="a"
            href="/"
            sx={{
              mr: 2,
              display: {xs: 'none', md: 'flex'},
              fontFamily: 'monospace',
              fontWeight: 700,
              letterSpacing: '.3rem',
              color: 'inherit',
              textDecoration: 'none'
            }}
          >
            Rack Monitor
          </Typography>
          <Typography variant="h6" component="div" sx={{flexGrow: 1}}>
            News
          </Typography>
          <Button color="inherit">Login</Button>
        </Toolbar>
      </AppBar>
    </Box>
  );
}

class Index extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      currDate: new Date()
    };
  }

  render() {
    return (
      <>

        <ButtonAppBar />
        <div style={{
          maxWidth: '1280px', padding: '0.75rem', display: 'block',
          marginLeft: 'auto', marginRight: 'auto', marginBottom: '3em'
        }}>
          <Grid container spacing={2}>
            <Grid xs={12} md={6} >
              <LiveImages />
            </Grid>
            <Grid xs={12} md={6} >
              <header className="d-flex align-items-center pb-2 mb-3 border-bottom">
                <a href="." className="d-flex align-items-center text-dark text-decoration-none">
                  <svg 
                    xmlns="http://www.w3.org/2000/svg" width="32" height="32" fill="currentColor"
                    className="bi bi-door-open-fill" viewBox="0 0 16 16"
                  >
                    <path d="M1.5 15a.5.5 0 0 0 0 1h13a.5.5 0 0 0 0-1H13V2.5A1.5 1.5 0 0 0 11.5 1H11V.5a.5.5 0 0 0-.57-.495l-7 1A.5.5 0 0 0 3 1.5V15H1.5zM11 2h.5a.5.5 0 0 1 .5.5V15h-1V2zm-2.5 8c-.276 0-.5-.448-.5-1s.224-1 .5-1 .5.448.5 1-.224 1-.5 1z"/>
                  </svg>
                  <span fontWeight="bold" className="fs-3">&nbsp;Door</span>
                </a>
              </header>
              <div><DoorState /></div>
            </Grid>
          </Grid>
        </div>
      </>
    );
  }
}

const container = document.getElementById('root');
const root = createRoot(container); // createRoot(container!) if you use TypeScript

root.render(<Index />);
