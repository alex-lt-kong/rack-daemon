import * as React from 'react';
import {createRoot} from 'react-dom/client';
import Slider from '@mui/material/Slider';
import axios from 'axios';
import Grid from '@mui/material/Grid'; // Grid version 1
import AppBar from '@mui/material/AppBar';
import Box from '@mui/material/Box';
import Toolbar from '@mui/material/Toolbar';
import Typography from '@mui/material/Typography';
import Button from '@mui/material/Button';
import Card from '@mui/material/Card';
import CardActions from '@mui/material/CardActions';
import CardContent from '@mui/material/CardContent';
//import MaterialReactTable from 'material-react-table';

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
    //should be memoized or stable
    const columns = useMemo(
        () => [
          {
            accessorKey: 'record_id', //normal accessorKey
            header: 'ID'
          },
          {
            accessorKey: 'record_time',
            header: 'Time'
          },
          {
            accessorKey: 'door_state',
            header: 'State'
          }
        ],
        []
    );
    return (
      <Card sx={{display: 'flex'}}>
        <Box sx={{display: 'flex', flexDirection: 'column'}}>
          <CardContent sx={{flex: '1 0 auto'}}>
            <Typography component="div" variant="h5">
              Door State
            </Typography>
          </CardContent>
          <Box sx={{display: 'flex', alignItems: 'center', pl: 1, pb: 1}}>
            {/*<MaterialReactTable columns={columns} data={this.state.doorStates} />*/}
          </Box>
        </Box>
      </Card>
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
        <Card sx={{maxWidth: 380}}>
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
    <Box sx={{flexGrow: 1, mb: '2rem'}}>
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
          maxWidth: '1280px', display: 'block',
          marginLeft: 'auto', marginRight: 'auto'
        }}>
          <Grid container spacing={2}>
            <Grid xs={12} md={4} >
              <LiveImages />
            </Grid>
            <Grid xs={12} md={8} >
              <DoorState />
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
